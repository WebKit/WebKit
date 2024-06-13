/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "HashMapImplInlines.h"
#include "JSImmutableButterfly.h"
#include "JSObject.h"

namespace JSC {

struct MapTraits {
    // DataTable:  { <Entry_0, Key_0>, <Entry_0 + 1, Val_0>, <Entry_0 + 2, Chain_0>, ... }
    static constexpr uint8_t EntrySize = 3;

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueData(Accessor* table, typename Accessor::TableIndex entryKeyIndex) { return table->getData(entryKeyIndex + 1); }

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueDataIfNeeded(Accessor* table, typename Accessor::TableIndex entryValueIndex) { return table->getData(entryValueIndex); }

    template<typename Accessor>
    ALWAYS_INLINE static void setValueDataIfNeeded(Accessor* table, VM& vm, typename Accessor::TableIndex entryValueIndex, JSValue value) { table->setData(vm, entryValueIndex, value); }

    template<typename Accessor>
    ALWAYS_INLINE static void copyValueDataIfNeeded(VM& vm, Accessor* baseTable, typename Accessor::TableIndex baseEntryValueIndex, Accessor* newTable, typename Accessor::TableIndex newEntryValueIndex)
    {
        JSValue baseValue = baseTable->getData(baseEntryValueIndex);
        ASSERT(baseTable->isValidValueData(vm, baseValue));
        newTable->setData(vm, newEntryValueIndex, baseValue);
    }

    template<typename Accessor>
    ALWAYS_INLINE static void deleteValueDataIfNeeded(Accessor* table, VM& vm, typename Accessor::TableIndex entryValueIndex) { table->deleteData(vm, entryValueIndex); }
};

struct SetTraits {
    // DataTable:  { <Entry_0, Key_0>, <Entry_0 + 1, Chain_0>, ... }
    static constexpr uint8_t EntrySize = 2;

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueData(Accessor* table, typename Accessor::TableIndex entryKeyIndex) { return table->getData(entryKeyIndex); }

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueDataIfNeeded(Accessor*, typename Accessor::TableIndex) { return {}; }

    template<typename Accessor>
    ALWAYS_INLINE static void setValueDataIfNeeded(Accessor*, VM&, typename Accessor::TableIndex, JSValue) {}

    template<typename Accessor>
    ALWAYS_INLINE static void copyValueDataIfNeeded(VM&, Accessor*, typename Accessor::TableIndex, Accessor*, typename Accessor::TableIndex) {}

    template<typename Accessor>
    ALWAYS_INLINE static void deleteValueDataIfNeeded(Accessor*, VM&, typename Accessor::TableIndex) {}
};

template<typename Traits>
class OrderedHashTable;

// Note that only ChainTable stores the real JSValues and the others are used as unsigned integer number wrapped in JSValue.
//
// ################ Non-Obsolete Table ################
//
//                      Count                                    Value(s)                                ValueType             WriteBarrier
//               --------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | AliveEntryCount                                                 | TableSize           | NO
//                         1          | DeletedEntryCount                                               | TableSize           | NO
//                         1          | Capacity                                                        | TableSize           | NO
//                         1          | IterationEntry                                                  | Entry               | NO
//                    BucketCount     | HashTable:  { <BucketIndex, ChainStartEntry>, ... }             | <TableIndex, Entry> | NO
//  TableEnd  -> Capacity * EntrySize | DataTable:  { <Entry_0, Data_0>, <Entry_0 + 1, Data_1>, ... }   | <Entry, JSValue>    | Yes
//
// ################ Obsolete Table from Rehash ################
//
//                      Count                                    Value(s)                                ValueType             WriteBarrier
//               --------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                       | TableSize           | Yes
//                         1          | DeletedEntryCount                                               | TableSize           | NO
//                         1          | Capacity                                                        | TableSize           | NO
//                         1          | IterationEntry                                                  | Entry               | NO
//                  DeletedEntryCount | DeletedEntries: [DeletedEntry_i, ...]                           | Entry               | NO
//  TableEnd  ->          ...         | NotUsed                                                         | ...
//
// ################ Obsolete Table from Clear ################
//
//                      Count                                    Value(s)                                ValueType             WriteBarrier
//               --------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                       | TableSize           | Yes
//                         1          | ClearedTableSentinel                                            | TableSize           | NO
//                         1          | Capacity                                                        | TableSize           | NO
//                         1          | IterationEntry                                                  | Entry               | NO
//  TableEnd  ->          ...         | NotUsed                                                         | ...
//
// The table is initialized with empty JSValues.
// 1. Found an empty entry while iterating the hash table means the current bucket is not taken by any chain.
// 2. Found an empty entry while iterating the chain table means the index entry is the end of the chain.
template<typename Traits>
class OrderedHashTableAccessor : public JSImmutableButterfly {
    using Base = JSImmutableButterfly;

public:
    using Accessor = OrderedHashTableAccessor<Traits>;
    using HashTable = OrderedHashTable<Traits>;
    using TableSize = uint32_t;
    using Entry = TableSize;
    using TableIndex = TableSize;

    static constexpr TableSize ClearedTableSentinel = -1;
    static constexpr TableIndex InvalidTableIndex = -1;
    static constexpr uint8_t EntrySize = Traits::EntrySize;
    static constexpr uint8_t ChainOffset = Traits::EntrySize - 1;

    // TODO: [2, 4] It seems [1, 8] is faster.
    static constexpr uint8_t LoadFactor = 1;
    static constexpr uint8_t InitialCapacity = 8;

    static_assert(EntrySize == MapTraits::EntrySize || EntrySize == SetTraits::EntrySize);

    OrderedHashTableAccessor() = delete;

    ALWAYS_INLINE static TableSize toNumber(JSValue number) { return static_cast<TableSize>(number.asInt32()); }
    ALWAYS_INLINE static JSValue toJSValue(Entry entry)
    {
        JSValue value = JSValue();
        OrderedHashTableTraits::set(&value, entry);
        return value;
    }

    ALWAYS_INLINE static bool isDeleted(VM& vm, JSValue value) { return value.isCell() && value.asCell() == vm.orderedHashTableDeletedValue(); }
    ALWAYS_INLINE bool isValidEntry(Entry entry) const { return entry < usedCapacity(); }
    ALWAYS_INLINE bool isValidEntry(JSValue entry) const { return !entry.isEmpty() && entry.isNumber() && toNumber(entry) < usedCapacity(); }
    ALWAYS_INLINE static bool isValidValueData(VM& vm, JSValue value) { return !value.isEmpty() && !isDeleted(vm, value); }
    ALWAYS_INLINE static bool isValidTableIndex(TableIndex index) { return index != InvalidTableIndex; }
    ALWAYS_INLINE static JSValue invalidTableIndex() { return toJSValue(InvalidTableIndex); }

    enum class IndexType : uint8_t {
        Any = 0,
        Default = 1,
        Bucket = 2,
        Chain = 3,
        Deleted = 4,
        Data = 5,
    };

    template<IndexType type>
    bool isValidIndex(TableIndex index) const
    {
        switch (type) {
        case IndexType::Default:
            return aliveEntryCountIndex() <= index && index <= iterationEntryIndex();
        case IndexType::Bucket:
            return hashTableStartIndex() <= index && index <= hashTableEndIndex();
        case IndexType::Chain:
            return dataTableStartIndex() <= index && index <= dataTableEndIndex() && isValidChainIndex(index);
        case IndexType::Data:
            return dataTableStartIndex() <= index && index <= dataTableEndIndex();
        case IndexType::Deleted:
            return deletedEntriesStartIndex() <= index && index <= deletedEntriesEndIndex();
        case IndexType::Any:
            return index < tableSize();
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    template<IndexType type = IndexType::Any>
    ALWAYS_INLINE JSValue get(TableIndex index) const
    {
        ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
        return toButterfly()->contiguous().atUnsafe(index).get();
    }
    template<IndexType type = IndexType::Any>
    ALWAYS_INLINE JSValue* slot(TableIndex index) const
    {
        ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
        return toButterfly()->contiguous().atUnsafe(index).slot();
    }

    template<IndexType type = IndexType::Any>
    ALWAYS_INLINE void set(VM& vm, TableIndex index, JSValue value)
    {
        ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
        ASSERT_UNUSED(vm, (type <= IndexType::Deleted && !isDeleted(vm, value)) || (type == IndexType::Data && isDeleted(vm, value)));
        return toButterfly()->contiguous().atUnsafe(index).setWithoutWriteBarrier(value);
    }
    template<IndexType type = IndexType::Any>
    ALWAYS_INLINE void set(TableIndex index, TableSize number)
    {
        ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
        OrderedHashTableTraits::set(slot<type>(index), number);
    }

    template<IndexType type = IndexType::Any>
    ALWAYS_INLINE void setWithWriteBarrier(VM& vm, TableIndex index, JSValue value)
    {
        ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
        ASSERT(type == IndexType::Data || index == aliveEntryCountIndex());
        toButterfly()->contiguous().atUnsafe(index).set(vm, this, value);
    }

    /* -------------------------------- AliveEntryCount, DeletedEntryCount, Capacity, and IterationEntry -------------------------------- */
    ALWAYS_INLINE static constexpr TableSize aliveEntryCountIndex() { return 0; }
    ALWAYS_INLINE JSValue* aliveEntryCountSlot() const { return slot<IndexType::Default>(aliveEntryCountIndex()); }
    ALWAYS_INLINE TableSize aliveEntryCount() const { return toNumber(*aliveEntryCountSlot()); }
    ALWAYS_INLINE void setAliveEntryCount(TableSize aliveEntryCount) { return set<IndexType::Default>(aliveEntryCountIndex(), aliveEntryCount); }
    ALWAYS_INLINE void incrementAliveEntryCount()
    {
        JSValue* number = aliveEntryCountSlot();
        ASSERT(toNumber(*number) <= capacity());
        OrderedHashTableTraits::increment(number);
    }
    ALWAYS_INLINE void decrementAliveEntryCount()
    {
        JSValue* number = aliveEntryCountSlot();
        ASSERT(toNumber(*number) <= capacity());
        OrderedHashTableTraits::decrement(number);
    }

    ALWAYS_INLINE static constexpr TableSize deletedEntryCountIndex() { return aliveEntryCountIndex() + 1; }
    ALWAYS_INLINE JSValue* deletedEntryCountSlot() const { return slot<IndexType::Default>(deletedEntryCountIndex()); }
    ALWAYS_INLINE TableSize deletedEntryCount() const { return toNumber(*deletedEntryCountSlot()); }
    ALWAYS_INLINE void setDeletedEntryCount(TableSize deletedEntryCount) { return set<IndexType::Default>(deletedEntryCountIndex(), deletedEntryCount); }
    ALWAYS_INLINE void incrementDeletedEntryCount()
    {
        JSValue* number = deletedEntryCountSlot();
        ASSERT(toNumber(*number) <= capacity());
        OrderedHashTableTraits::increment(number);
    }

    ALWAYS_INLINE static constexpr TableSize capacityIndex() { return deletedEntryCountIndex() + 1; }
    ALWAYS_INLINE TableSize capacity() const { return toNumber(get<IndexType::Default>(capacityIndex())); }
    ALWAYS_INLINE void setCapacity(TableSize capacity) { set<IndexType::Default>(capacityIndex(), capacity); }
    ALWAYS_INLINE TableSize usedCapacity() const { return aliveEntryCount() + deletedEntryCount(); }

    ALWAYS_INLINE static constexpr TableSize iterationEntryIndex() { return capacityIndex() + 1; }
    ALWAYS_INLINE JSValue* iterationEntrySlot() const { return slot<IndexType::Default>(iterationEntryIndex()); }
    ALWAYS_INLINE Entry iterationEntry() const { return toNumber(*iterationEntrySlot()); }
    ALWAYS_INLINE void updateIterationEntry(Entry entry) { return set<IndexType::Default>(iterationEntryIndex(), entry); }

    /* -------------------------------- Hash table -------------------------------- */
    ALWAYS_INLINE static constexpr TableSize bucketCount(TableSize capacity) { return capacity / LoadFactor; }
    ALWAYS_INLINE TableSize bucketCount() const { return bucketCount(capacity()); }

    ALWAYS_INLINE static constexpr TableSize hashTableStartIndex() { return iterationEntryIndex() + 1; }

    ALWAYS_INLINE static constexpr TableSize hashTableEndIndex(TableSize capacity) { return hashTableStartIndex() + bucketCount(capacity) - 1; }
    ALWAYS_INLINE TableSize hashTableEndIndex() const { return hashTableEndIndex(capacity()); }

    ALWAYS_INLINE static TableIndex bucketIndex(TableIndex hashTableStartIndex, TableSize bucketCount, TableSize hash) { return hashTableStartIndex + (hash & (bucketCount - 1)); }
    ALWAYS_INLINE TableIndex bucketIndex(TableSize hash) { return hashTableStartIndex() + (hash & (bucketCount() - 1)); }

    ALWAYS_INLINE JSValue getChainStartEntry(TableIndex index) { return get<IndexType::Bucket>(index); }
    ALWAYS_INLINE void setChainStartEntry(TableIndex index, Entry entry)
    {
        ASSERT(isValidEntry(entry));
        return set<IndexType::Bucket>(index, entry);
    }

    /* -------------------------------- Data table -------------------------------- */
    ALWAYS_INLINE static TableSize dataTableSize(TableSize capacity) { return capacity * EntrySize; }
    ALWAYS_INLINE TableSize dataTableSize() const { return dataTableSize(capacity()); }

    ALWAYS_INLINE static TableSize dataTableStartIndex(TableSize capacity) { return hashTableEndIndex(capacity) + 1; }
    ALWAYS_INLINE TableSize dataTableStartIndex() const { return dataTableStartIndex(capacity()); }

    ALWAYS_INLINE static TableSize dataTableEndIndex(TableSize capacity) { return dataTableStartIndex(capacity) + dataTableSize(capacity) - 1; }
    ALWAYS_INLINE TableSize dataTableEndIndex() const { return dataTableEndIndex(capacity()); }

    ALWAYS_INLINE JSValue getData(TableIndex index) { return get<IndexType::Data>(index); }
    ALWAYS_INLINE void setData(VM& vm, TableIndex index, JSValue value) { setWithWriteBarrier<IndexType::Data>(vm, index, value); }

    ALWAYS_INLINE JSValue getChain(TableIndex index) { return get<IndexType::Chain>(index); }
    ALWAYS_INLINE void setChain(VM& vm, TableIndex index, JSValue value) { return set<IndexType::Chain>(vm, index, value); }

    ALWAYS_INLINE void deleteData(VM& vm, TableIndex index) { return set<IndexType::Data>(vm, index, vm.orderedHashTableDeletedValue()); }

    ALWAYS_INLINE void addToChain(VM& vm, TableIndex bucketIndex, Entry newChainStartEntry, TableIndex newChainStartEntryKeyIndex)
    {
        JSValue prevChainStartEntry = getChainStartEntry(bucketIndex);
        setChainStartEntry(bucketIndex, newChainStartEntry);
        TableIndex newChainStartEntryChainIndex = newChainStartEntryKeyIndex + ChainOffset;
        setChain(vm, newChainStartEntryChainIndex, prevChainStartEntry);
    }

    ALWAYS_INLINE static TableIndex dataStartIndex(TableIndex dataTableStartIndex, Entry entry)
    {
        return dataTableStartIndex + entry * EntrySize;
    }

    ALWAYS_INLINE bool isValidChainIndex(TableIndex index) const
    {
        // ChainIndex = DataTableStartIndex + (Entry * EntrySize) + (EntrySize - 1)
        return isValidEntry((index - dataTableStartIndex() - (EntrySize - 1)) / EntrySize);
    }

    /* -------------------------------- Overall table -------------------------------- */
    ALWAYS_INLINE static constexpr TableSize tableSize(TableSize capacity)
    {
        TableSize result = 4 /* AliveEntryCount, DeletedEntryCount, Capacity, and IterationEntry */
            + capacity / LoadFactor /* BucketCount */
            + capacity * EntrySize; /* DataTableSize */
        ASSERT(result == tableSizeSlow(capacity));
        return result;
    }
    ALWAYS_INLINE static TableSize tableSizeSlow(TableSize capacity) { return dataTableEndIndex(capacity) + 1; }
    ALWAYS_INLINE TableSize tableSize() const
    {
        TableSize size = length();
        ASSERT(size == tableSize(capacity()) && size == tableSizeSlow(capacity()));
        return size;
    }

    /* -------------------------------- Obsolete table -------------------------------- */
    ALWAYS_INLINE bool isObsolete() const { return !!nextTable(); }
    ALWAYS_INLINE Accessor* nextTable() const
    {
        JSValue* value = aliveEntryCountSlot();
        if (value->isCell()) {
            ASSERT(jsDynamicCast<Base*>(*value));
            return static_cast<Accessor*>(jsCast<Base*>(*value));
        }
        return nullptr;
    }
    ALWAYS_INLINE void setNextTable(VM& vm, Accessor* next) { setWithWriteBarrier<IndexType::Default>(vm, aliveEntryCountIndex(), next); }

    ALWAYS_INLINE TableIndex deletedEntriesStartIndex() const { return hashTableStartIndex(); }
    ALWAYS_INLINE TableIndex deletedEntriesEndIndex() const { return hashTableStartIndex() + deletedEntryCount() - 1; }
    ALWAYS_INLINE Entry getDeletedEntry(TableIndex deletedEntriesIndex) const { return toNumber(get<IndexType::Deleted>(deletedEntriesIndex)); }
    ALWAYS_INLINE void setDeletedEntry(TableIndex deletedEntriesIndex, Entry deletedEntry) { set<IndexType::Deleted>(deletedEntriesIndex, deletedEntry); }

    ALWAYS_INLINE bool isCleared() const { return deletedEntryCount() == ClearedTableSentinel; }
    ALWAYS_INLINE void setClearedTableSentinel() { setDeletedEntryCount(ClearedTableSentinel); }

    /* ------------------------------------------------------------------------------------ */

    ALWAYS_INLINE static Base* tryCreate(VM& vm, int length)
    {
        return Base::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length);
    }
    ALWAYS_INLINE static Accessor* tryCreate(JSGlobalObject* globalObject, TableSize aliveEntryCount = 0, TableSize deletedEntryCount = 0, TableSize capacity = InitialCapacity)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!(capacity & (capacity - 1)));

        TableSize length = tableSize(capacity);
        if (UNLIKELY(length > IndexingHeader::maximumLength)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        Base* butterfly = tryCreate(vm, length);
        if (UNLIKELY(!butterfly)) {
            throwOutOfMemoryError(globalObject, scope);
            return nullptr;
        }

        Accessor* table = static_cast<Accessor*>(butterfly);
        table->setAliveEntryCount(aliveEntryCount);
        table->setDeletedEntryCount(deletedEntryCount);
        table->setCapacity(capacity);
        return table;
    }

    enum class UpdateDeletedEntries : uint8_t {
        Yes,
        No
    };
    template<UpdateDeletedEntries update = UpdateDeletedEntries::No>
    ALWAYS_INLINE static Accessor* copyImpl(JSGlobalObject* globalObject, Accessor* baseTable, TableSize newCapacity)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        TableSize baseCapacity = baseTable->capacity();
        TableSize baseUsedCapacity = baseTable->usedCapacity();
        TableSize baseAliveEntryCount = baseTable->aliveEntryCount();
        ASSERT(baseTable && !baseTable->isObsolete());
        ASSERT_UNUSED(baseAliveEntryCount, newCapacity >= std::max(static_cast<TableSize>(InitialCapacity), baseAliveEntryCount));
        ASSERT_UNUSED(baseCapacity, baseUsedCapacity <= baseCapacity);

        Accessor* newTable = tryCreate(globalObject, baseTable->aliveEntryCount(), 0, newCapacity);
        RETURN_IF_EXCEPTION(scope, nullptr);

        TableSize newEntry = 0;
        TableIndex baseDataTableStartIndex = baseTable->dataTableStartIndex();
        TableIndex baseDeletedEntriesIndex = 0;
        TableIndex baseDeletedEntriesStartIndex = baseTable->deletedEntriesStartIndex();
        TableIndex newDataTableStartIndex = newTable->dataTableStartIndex();
        TableIndex newHashTableStartIndex = newTable->hashTableStartIndex();
        TableIndex newBucketCount = newTable->bucketCount();
        for (Entry baseEntry = 0; baseEntry < baseUsedCapacity; ++baseEntry) {
            TableIndex baseEntryKeyIndex = dataStartIndex(baseDataTableStartIndex, baseEntry);
            JSValue baseKey = baseTable->getData(baseEntryKeyIndex);
            ASSERT(!baseKey.isEmpty());

            // Step 1: Copy DataTable only for the alive entries.
            if (isDeleted(vm, baseKey)) {
                if constexpr (update == UpdateDeletedEntries::Yes)
                    baseTable->setDeletedEntry(baseDeletedEntriesStartIndex + baseDeletedEntriesIndex++, baseEntry);
                continue;
            }

            // Step 2: Copy the key and value from the base table to the new table->
            TableIndex newEntryKeyIndex = dataStartIndex(newDataTableStartIndex, newEntry);
            newTable->setData(vm, newEntryKeyIndex, baseKey);
            Traits::template copyValueDataIfNeeded(vm, baseTable, baseEntryKeyIndex + 1, newTable, newEntryKeyIndex + 1);

            // Step 3: Compute for the hash value and add to the chain in the new table-> Note that the
            // key stored in the base table is already normalized.
            TableSize hash = jsMapHash(globalObject, vm, baseKey);
            RETURN_IF_EXCEPTION(scope, nullptr);
            TableIndex newBucketIndex = bucketIndex(newHashTableStartIndex, newBucketCount, hash);
            newTable->addToChain(vm, newBucketIndex, newEntry, newEntryKeyIndex);

            ++newEntry;
        }

        return newTable;
    }
    static Base* copy(JSGlobalObject* globalObject, Base* base)
    {
        Accessor* baseTable = static_cast<Accessor*>(base);
        Accessor* newTable = copyImpl<>(globalObject, baseTable, baseTable->capacity());
        return static_cast<Base*>(newTable);
    }

    ALWAYS_INLINE void setOwnerStorage(VM& vm, HashTable* owner, Accessor* newTable)
    {
        ASSERT(owner->m_storage.get() == this && nextTable() == newTable);
        owner->m_storage.set(vm, owner, static_cast<Base*>(newTable));
    }

    enum class SetOwnerStorage : uint8_t {
        Yes,
        No
    };
    template<SetOwnerStorage update>
    ALWAYS_INLINE Accessor* rehash(JSGlobalObject* globalObject, HashTable* owner, TableSize newCapacity)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Rehash> Before: oldTable=", Dump(this, vm));

        Accessor* newTable = copyImpl<UpdateDeletedEntries::Yes>(globalObject, this, newCapacity);
        RETURN_IF_EXCEPTION(scope, {});

        setNextTable(vm, newTable);
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Rehash> Rehashed: oldTable=", Dump(this, vm), "\nnewTable=", Dump(newTable, vm));

        if constexpr (update == SetOwnerStorage::Yes)
            setOwnerStorage(vm, owner, newTable);
        return newTable;
    }

    ALWAYS_INLINE void clear(JSGlobalObject* globalObject, HashTable* owner)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Clear> Before: oldTable=", Dump(this, vm));

        Accessor* newTable = tryCreate(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        setClearedTableSentinel();
        setNextTable(vm, newTable);
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Clear> Cleared: oldTable=", Dump(this, vm), "\nnewTable=", Dump(newTable, vm));

        owner->m_storage.set(vm, owner, static_cast<Base*>(newTable));
    }

    ALWAYS_INLINE bool has(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());

        if (!aliveEntryCount())
            return false;

        key = normalizeMapKey(key);
        TableSize hash = jsMapHash(globalObject, vm, key);
        RETURN_IF_EXCEPTION(scope, {});

        TableIndex bucketIndex = this->bucketIndex(hash);
        JSValue entry = getChainStartEntry(bucketIndex);
        TableIndex dataTableStartIndex = this->dataTableStartIndex();
        while (!entry.isEmpty()) {
            TableIndex entryKeyIndex = dataStartIndex(dataTableStartIndex, toNumber(entry));
            JSValue entryKey = getData(entryKeyIndex);
            if (!isDeleted(vm, entryKey) && areKeysEqual(globalObject, key, entryKey))
                return true;
            entry = getChain(entryKeyIndex + ChainOffset);
        }

        return false;
    }

    ALWAYS_INLINE bool has(JSGlobalObject* globalObject, JSValue normalizedKey, uint32_t hash)
    {
        ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);
        VM& vm = getVM(globalObject);

        if (!aliveEntryCount())
            return false;

        TableIndex bucketIndex = this->bucketIndex(hash);
        JSValue entry = getChainStartEntry(bucketIndex);
        TableIndex dataTableStartIndex = this->dataTableStartIndex();
        while (!entry.isEmpty()) {
            TableIndex entryKeyIndex = dataStartIndex(dataTableStartIndex, toNumber(entry));
            JSValue entryKey = getData(entryKeyIndex);
            if (!isDeleted(vm, entryKey) && areKeysEqual(globalObject, normalizedKey, entryKey))
                return true;
            entry = getChain(entryKeyIndex + ChainOffset);
        }

        return false;
    }

    // Return normalizedKey, bucketIndex, entryKeyIndex
    ALWAYS_INLINE std::tuple<JSValue, TableIndex, TableIndex> find(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());

        if (!aliveEntryCount()) {
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> No entries in table=", RawPointer(this));
            return { JSValue(), InvalidTableIndex, InvalidTableIndex };
        }

        key = normalizeMapKey(key);
        TableSize hash = jsMapHash(globalObject, vm, key);
        RETURN_IF_EXCEPTION(scope, {});
        return find(globalObject, key, hash);
    }

    // Return normalizedKey, bucketIndex, entryKeyIndex
    ALWAYS_INLINE std::tuple<JSValue, TableIndex, TableIndex> find(JSGlobalObject* globalObject, JSValue normalizedKey, TableSize hash)
    {
        VM& vm = getVM(globalObject);
        TableIndex bucketIndex = this->bucketIndex(hash);
        ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);

        JSValue entry = getChainStartEntry(bucketIndex);
        TableIndex dataTableStartIndex = this->dataTableStartIndex();
        while (!entry.isEmpty()) {
            TableIndex entryKeyIndex = dataStartIndex(dataTableStartIndex, toNumber(entry));
            JSValue entryKey = getData(entryKeyIndex);
            if (!isDeleted(vm, entryKey) && areKeysEqual(globalObject, normalizedKey, entryKey)) {
                dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> Found entry=", entry, " in the chain with bucketIndex=", bucketIndex, "  table=", RawPointer(this));
                return { normalizedKey, bucketIndex, entryKeyIndex };
            }
            entry = getChain(entryKeyIndex + ChainOffset);
        }

        dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> Not found the entry in the chain with bucketIndex=", bucketIndex, "  table=", RawPointer(this));
        return { normalizedKey, bucketIndex, InvalidTableIndex };
    }

    ALWAYS_INLINE Accessor* expandIfNeeded(JSGlobalObject* globalObject, HashTable* owner)
    {
        ASSERT(!isObsolete());
        TableSize currentCapacity = capacity();
        if (usedCapacity() < currentCapacity)
            return this;

        TableSize newCapacity = currentCapacity << 1;
        if (deletedEntryCount() >= (currentCapacity >> 1))
            newCapacity = currentCapacity; // No need to expanded. Just clear the deleted entries.
        return rehash<SetOwnerStorage::No>(globalObject, owner, newCapacity);
    }

    template<typename FindKeyFunctor>
    ALWAYS_INLINE void addImpl(JSGlobalObject* globalObject, HashTable* owner, JSValue key, JSValue value, const FindKeyFunctor& findKeyFunctor)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Before: table=", Dump(this, vm));

        auto [normalizedKey, bucketIndex, entryKeyIndex] = findKeyFunctor();
        RETURN_IF_EXCEPTION(scope, void());

        if (Accessor::isValidTableIndex(entryKeyIndex)) {
            Traits::template setValueDataIfNeeded(this, vm, entryKeyIndex + 1, value);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Found and set entryKeyIndex=", entryKeyIndex, " bucketIndex=", bucketIndex, " table=", Dump(this, vm));
        } else {
            Accessor* table = expandIfNeeded(globalObject, owner);
            RETURN_IF_EXCEPTION(scope, void());

            Entry newEntry = table->usedCapacity();
            TableIndex newEntryKeyIndex = dataStartIndex(table->dataTableStartIndex(), newEntry);
            table->incrementAliveEntryCount();

            bool isFirstAliveEntry = normalizedKey.isEmpty();
            if (isFirstAliveEntry)
                normalizedKey = normalizeMapKey(key);

            bool rehashed = this != table;
            if (rehashed || isFirstAliveEntry) {
                ASSERT(normalizeMapKey(key) == normalizedKey);
                TableSize hash = jsMapHash(globalObject, vm, normalizedKey);
                RETURN_IF_EXCEPTION(scope, void());
                bucketIndex = table->bucketIndex(hash);
            }

            ASSERT(table->isValidIndex<IndexType::Bucket>(bucketIndex));
            table->addToChain(vm, bucketIndex, newEntry, newEntryKeyIndex);
            table->setData(vm, newEntryKeyIndex, normalizedKey);
            Traits::template setValueDataIfNeeded(table, vm, newEntryKeyIndex + 1, value);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Added newEntry=", newEntry, " entryKeyIndex=", entryKeyIndex, " bucketIndex=", bucketIndex, " table=", Dump(table, vm));

            if (rehashed)
                setOwnerStorage(vm, owner, table);
        }
    }
    ALWAYS_INLINE void add(JSGlobalObject* globalObject, HashTable* owner, JSValue key, JSValue value)
    {
        addImpl(globalObject, owner, key, value, [&]() ALWAYS_INLINE_LAMBDA {
            return find(globalObject, key);
        });
    }
    ALWAYS_INLINE void addNormalized(JSGlobalObject* globalObject, HashTable* owner, JSValue normalizedKey, JSValue value, TableSize hash)
    {
        ASSERT(normalizeMapKey(normalizedKey) == normalizedKey);
        addImpl(globalObject, owner, normalizedKey, value, [&]() ALWAYS_INLINE_LAMBDA {
            return find(globalObject, normalizedKey, hash);
        });
    }

    ALWAYS_INLINE void shrinkIfNeed(JSGlobalObject* globalObject, HashTable* owner)
    {
        ASSERT(!isObsolete());
        TableSize currentCapacity = capacity();
        if (aliveEntryCount() >= (currentCapacity >> 2))
            return;
        if (currentCapacity == InitialCapacity)
            return;
        rehash<SetOwnerStorage::Yes>(globalObject, owner, currentCapacity >> 1);
    }

    template<typename FindKeyFunctor>
    ALWAYS_INLINE bool removeImpl(JSGlobalObject* globalObject, HashTable* owner, const FindKeyFunctor& findKeyFunctor)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());

        auto [_, bucketIndex, entryKeyIndex] = findKeyFunctor();
        RETURN_IF_EXCEPTION(scope, false);

        if (!Accessor::isValidTableIndex(entryKeyIndex))
            return false;

        deleteData(vm, entryKeyIndex);
        Traits::template deleteValueDataIfNeeded(this, vm, entryKeyIndex + 1);
        incrementDeletedEntryCount();
        decrementAliveEntryCount();

        shrinkIfNeed(globalObject, owner);
        RETURN_IF_EXCEPTION(scope, false);
        dataLogLnIf(Options::dumpOrderedHashTable(), "<Remove> Removed entryKeyIndex=", entryKeyIndex, " bucketIndex=", bucketIndex, " table=", Dump(this, vm));
        return true;
    }
    ALWAYS_INLINE bool remove(JSGlobalObject* globalObject, HashTable* owner, JSValue key)
    {
        return removeImpl(globalObject, owner, [&]() ALWAYS_INLINE_LAMBDA {
            return find(globalObject, key);
        });
    }
    ALWAYS_INLINE bool removeNormalized(JSGlobalObject* globalObject, HashTable* owner, JSValue normalizedKey, TableSize hash)
    {
        ASSERT(normalizeMapKey(normalizedKey) == normalizedKey);
        return removeImpl(globalObject, owner, [&]() ALWAYS_INLINE_LAMBDA {
            return find(globalObject, normalizedKey, hash);
        });
    }

    ALWAYS_INLINE JSValue getKey(Entry entry)
    {
        ASSERT(!isObsolete());
        return getData(dataStartIndex(dataTableStartIndex(), entry));
    }

    ALWAYS_INLINE JSValue getValue(Entry entry)
    {
        ASSERT(!isObsolete() && EntrySize == 3);
        return getData(dataStartIndex(dataTableStartIndex(), entry) + 1);
    }

    ALWAYS_INLINE JSCell* nextAndUpdateIterationEntry(VM& vm, Entry entry)
    {
        auto [newTable, nextEntry] = nextTransition(vm, entry);
        if (!newTable)
            return vm.orderedHashTableSentinel();
        updateIterationEntry(nextEntry);
        return newTable;
    }

    ALWAYS_INLINE JSValue getIterationEntry() { return toJSValue(iterationEntry()); }
    ALWAYS_INLINE JSValue getIterationEntryKey() { return getKey(iterationEntry()); }
    ALWAYS_INLINE JSValue getIterationEntryValue() { return getValue(iterationEntry()); }

    template<typename Functor>
    ALWAYS_INLINE Accessor* transit(const Functor& functor)
    {
        Accessor* table = this;
        if (!table)
            return nullptr;

        while (table->isObsolete()) {
            functor(table);
            table = table->nextTable();
        }
        return table;
    }
    ALWAYS_INLINE Accessor* transit(Entry& from)
    {
        return transit([&](Accessor* obsoleteTale) ALWAYS_INLINE_LAMBDA {
            if (!from)
                return;

            if (obsoleteTale->isCleared()) {
                from = 0;
                return;
            }

            TableIndex deletedEntryCount = obsoleteTale->deletedEntryCount();
            if (!deletedEntryCount)
                return;

            TableIndex deletedEntriesStartIndex = obsoleteTale->deletedEntriesStartIndex();
            TableIndex deletedEntriesEnd = deletedEntriesStartIndex + deletedEntryCount;
            TableSize tmpFrom = from;
            for (TableIndex i = deletedEntriesStartIndex; i < deletedEntriesEnd; ++i) {
                Entry deletedEntry = obsoleteTale->getDeletedEntry(i);
                if (deletedEntry >= tmpFrom)
                    break;
                --from;
            }
        });
    }

    // This should be only used for iteration. And it should be guarded by the storage sentinel check.
    ALWAYS_INLINE std::tuple<Accessor*, Entry> nextTransition(VM& vm, Entry from)
    {
        Accessor* table = transit(from);
        TableSize usedCapacity = table->usedCapacity();
        TableIndex dataTableStartIndex = table->dataTableStartIndex();
        for (Entry entry = from; entry < usedCapacity; ++entry) {
            TableIndex entryKeyIndex = dataStartIndex(dataTableStartIndex, entry);
            JSValue key = table->getData(entryKeyIndex);
            if (isDeleted(vm, key))
                continue;
            return { table, entry };
        }
        return {};
    }
    std::tuple<Accessor*, JSValue, JSValue, JSValue> nextTransitionAll(VM& vm, Entry from)
    {
        Accessor* table = transit(from);
        ASSERT(table && !table->isObsolete());
        TableSize usedCapacity = table->usedCapacity();
        TableIndex dataTableStartIndex = table->dataTableStartIndex();
        for (Entry entry = from; entry < usedCapacity; ++entry) {
            TableIndex entryKeyIndex = dataStartIndex(dataTableStartIndex, entry);
            JSValue key = table->getData(entryKeyIndex);
            if (isDeleted(vm, key))
                continue;

            JSValue value = Traits::template getValueDataIfNeeded(table, entryKeyIndex + 1);
            return { table, key, value, JSValue(entry) };
        }
        return {};
    }

    class Dump {
    public:
        Dump(Accessor* table, VM& vm)
            : m_table(table)
            , m_vm(vm)
        {
        }

        void dump(PrintStream& out) const
        {
            if (m_table->isObsolete()) {
                out.print("\nthis=", RawPointer(m_table), "\n");
                out.print("NextTable=", RawPointer(m_table->nextTable()), "\n");
                out.print(m_table->isCleared() ? "ClearedTableSentinel=" : "DeletedEntryCount=", m_table->deletedEntryCount(), "\n");
                out.print("Capacity=", m_table->capacity(), "\n");

                if (m_table->isCleared())
                    return;

                out.print("DeletedEntries=[");
                for (TableIndex i = m_table->deletedEntriesStartIndex(); i <= m_table->deletedEntriesEndIndex(); ++i) {
                    JSValue entry = m_table->get<IndexType::Deleted>(i);
                    out.print(m_table->toNumber(entry));
                    if (i != m_table->deletedEntriesEndIndex())
                        out.print(", ");
                }
                ASSERT(m_table->deletedEntryCount() == m_table->deletedEntriesEndIndex() - m_table->deletedEntriesStartIndex() + 1);
                out.print("] DeletedEntryCount=", m_table->deletedEntryCount(), "\n");
                return;
            }

            out.print("\nthis=", RawPointer(m_table), "\n");
            out.print("AliveEntryCount=", m_table->aliveEntryCount(), "\n");
            out.print("DeletedEntryCount=", m_table->deletedEntryCount(), "\n");
            out.print("Capacity=", m_table->capacity(), "\n");

            out.print("HashTable=[");
            for (TableIndex i = m_table->hashTableStartIndex(); i <= m_table->hashTableEndIndex(); ++i) {
                JSValue entry = m_table->get<IndexType::Bucket>(i);
                if (entry.isEmpty())
                    out.print("EMPTY");
                else
                    out.print(m_table->toNumber(entry));
                if (i != m_table->hashTableEndIndex())
                    out.print(", ");
            }
            ASSERT(m_table->bucketCount() == m_table->hashTableEndIndex() - m_table->hashTableStartIndex() + 1);
            out.print("] HashTableSize=", m_table->bucketCount(), "\n");

            out.print("DataTable=[\n");
            for (TableIndex i = m_table->dataTableStartIndex(); i <= m_table->dataTableEndIndex(); ++i) {
                JSValue value = m_table->get<IndexType::Data>(i);
                out.print("    ");
                if (value.isEmpty())
                    out.print("EMPTY");
                else if (isDeleted(m_vm, value))
                    out.print("DELETED");
                else
                    out.print(value);
                out.print(",\n");
            }
            ASSERT(m_table->dataTableSize() == m_table->dataTableEndIndex() - m_table->dataTableStartIndex() + 1);
            out.print("] DataTableSize=", m_table->dataTableSize(), "\n");
        }

        Accessor* m_table;
        VM& m_vm;
    };
};

} // namespace JSC
