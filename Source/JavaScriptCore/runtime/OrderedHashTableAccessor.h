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

#include "HashMapHelper.h"
#include "JSImmutableButterfly.h"
#include "JSObject.h"

namespace JSC {

struct MapTraits {
    // DataTable:  [ Key_0, Val_0, NextInChain_0, Key_1, Val_1, NextKeyIndexInChain_1, ... ]
    //             | <-------- Entry_0 -------> | <-------- Entry_1 -------> |
    static constexpr uint8_t EntrySize = 3;

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueData(Accessor* table, typename Accessor::TableIndex entryKeyIndex) { return table->getKeyOrValueData(entryKeyIndex + 1); }

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueDataIfNeeded(Accessor* table, typename Accessor::TableIndex entryValueIndex) { return table->getKeyOrValueData(entryValueIndex); }

    template<typename Accessor>
    ALWAYS_INLINE static void setValueDataIfNeeded(Accessor* table, VM& vm, typename Accessor::TableIndex entryValueIndex, JSValue value) { table->setKeyOrValueData(vm, entryValueIndex, value); }

    template<typename Accessor>
    ALWAYS_INLINE static void copyValueDataIfNeeded(VM& vm, Accessor* baseTable, typename Accessor::TableIndex baseEntryValueIndex, Accessor* newTable, typename Accessor::TableIndex newEntryValueIndex)
    {
        JSValue baseValue = baseTable->getKeyOrValueData(baseEntryValueIndex);
        ASSERT(baseTable->isValidValueData(vm, baseValue));
        newTable->setKeyOrValueData(vm, newEntryValueIndex, baseValue);
    }

    template<typename Accessor>
    ALWAYS_INLINE static void deleteValueDataIfNeeded(Accessor* table, VM& vm, typename Accessor::TableIndex entryValueIndex) { table->deleteData(vm, entryValueIndex); }
};

struct SetTraits {
    // DataTable:  [ Key_0, NextInChain_0, Key_1, NextKeyIndexInChain_1, ... ]
    //             | <---- Entry_0 ----> | <---- Entry_1 ----> |
    static constexpr uint8_t EntrySize = 2;

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueData(Accessor* table, typename Accessor::TableIndex entryKeyIndex) { return table->getKeyOrValueData(entryKeyIndex); }

    template<typename Accessor>
    ALWAYS_INLINE static JSValue getValueDataIfNeeded(Accessor*, typename Accessor::TableIndex) { return { }; }

    template<typename Accessor>
    ALWAYS_INLINE static void setValueDataIfNeeded(Accessor*, VM&, typename Accessor::TableIndex, JSValue) { }

    template<typename Accessor>
    ALWAYS_INLINE static void copyValueDataIfNeeded(VM&, Accessor*, typename Accessor::TableIndex, Accessor*, typename Accessor::TableIndex) { }

    template<typename Accessor>
    ALWAYS_INLINE static void deleteValueDataIfNeeded(Accessor*, VM&, typename Accessor::TableIndex) { }
};

template<typename Traits>
class OrderedHashTable;

namespace OrderedHashTableAccessorInternal {
static constexpr bool verbose = false;
}

// ################ NonObsolete Table ################
//
//                      Count                                    Value(s)                           ValueType                  WriteBarrier
//               -------------------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | AliveEntryCount                                            | TableSize                | NO
//                         1          | DeletedEntryCount                                          | TableSize                | NO
//                         1          | Capacity                                                   | TableSize                | NO
//                         1          | IterationEntry                                             | Entry                    | NO
//                    BucketCount     | HashTable:  { <BucketIndex, ChainStartKeyIndex>, ... }     | <TableIndex, TableIndex> | NO
//  TableEnd  -> Capacity * EntrySize | DataTable:  { <Entry_0, Data_0>, <Entry_1, Data_1>, ... }  | <TableIndex, JSValue>    | Yes (Key or Value)
//
// ################ Obsolete Table from Rehash ################
//
//                      Count                                    Value(s)                            ValueType                  WriteBarrier
//               -------------------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                   | JSCell                   | Yes
//                         1          | DeletedEntryCount                                           | TableSize                | NO
//                         1          | Capacity                                                    | TableSize                | NO
//                         1          | IterationEntry                                              | Entry                    | NO
//                  DeletedEntryCount | DeletedEntries: [DeletedEntry_i, ...]                       | Entry                    | NO
//  TableEnd  ->          ...         | NotUsed                                                     | ...
//
// ################ Obsolete Table from Clear ################
//
//                      Count                                    Value(s)                            ValueType                  WriteBarrier
//               -------------------------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                   | JSCell                   | Yes
//                         1          | ClearedTableSentinel                                        | TableSize                | NO
//                         1          | Capacity                                                    | TableSize                | NO
//                         1          | IterationEntry                                              | Entry                    | NO
//  TableEnd  ->          ...         | NotUsed                                                     | ...
//
//
// This is based on the idea of CloseTable introduced in https://wiki.mozilla.org/User:Jorend/Deterministic_hash_tables.
//
// NonObsolete Table: A CloseTable flattened into a single array.
//
// - AliveEntryCount: The number of alive entries in the table.
// - DeletedEntryCount: The number of deleted entries in the table.
// - Capacity: The capacity of entries in the table.
// - IterationEntry: The temporary index only used for iteration.
// - HashTable: CloseTable is based on closed hashing. So, the each bucket in the HashTable points to the start EntryKeyIndex in the collision chain.
// - DataTable: The actual data table holds the tuples of key, value, and NextKeyIndexInChain for each entry.
//
//      Iterator                                   Map/Set
//          |                                         |
//          v                                         v
//    ObsoleteTable_0 --> ObsoleteTable_1 --> NonObsoleteTable_2
//
// Obsolete Table: Each rehash and clear would create a new table and update the obsolete one with additional info. Those info are used to
// transit the iterator (pointing to the obsolete table) to the latest alive table.
//
// - NextTable: A transition from the obsolete tables to the latest and alive one.
// - DeletedEntries: An array of the previously deleted entries used for updating iterator's index.
// - ClearedTableSentinel: The sentinel to indicate whether the obsolete table is retired due to a clearance.
//
// Note that all elements in the JSImmutableButterfly are in JSValue type. However, only the key and value in the DataTable are real JSValues.
// The others are used as unsigned integers wrapped by JSValue.
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

    // FIXME: Maybe try LoadFactor = 2 and InitialCapacity = 4.
    static constexpr uint8_t LoadFactor = 1;
    static constexpr uint8_t InitialCapacity = 8;
    static constexpr uint8_t ExpandFactor = 2;

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

    ALWAYS_INLINE JSValue getChainStartKeyIndex(TableIndex index) { return get<IndexType::Bucket>(index); }
    ALWAYS_INLINE void setChainStartKeyIndex(TableIndex index, TableIndex keyIndex) { return set<IndexType::Bucket>(index, keyIndex); }

    /* -------------------------------- Data table -------------------------------- */
    ALWAYS_INLINE static TableSize dataTableSize(TableSize capacity) { return capacity * EntrySize; }
    ALWAYS_INLINE TableSize dataTableSize() const { return dataTableSize(capacity()); }

    ALWAYS_INLINE static TableSize dataTableStartIndex(TableSize capacity) { return hashTableEndIndex(capacity) + 1; }
    ALWAYS_INLINE TableSize dataTableStartIndex() const { return dataTableStartIndex(capacity()); }

    ALWAYS_INLINE static TableSize dataTableEndIndex(TableSize capacity) { return dataTableStartIndex(capacity) + dataTableSize(capacity) - 1; }
    ALWAYS_INLINE TableSize dataTableEndIndex() const { return dataTableEndIndex(capacity()); }

    ALWAYS_INLINE JSValue getKeyOrValueData(TableIndex index) { return get<IndexType::Data>(index); }
    ALWAYS_INLINE void setKeyOrValueData(VM& vm, TableIndex index, JSValue value) { setWithWriteBarrier<IndexType::Data>(vm, index, value); }

    ALWAYS_INLINE JSValue getChainData(TableIndex index) { return get<IndexType::Chain>(index); }
    ALWAYS_INLINE void setChainData(VM& vm, TableIndex index, JSValue value) { return set<IndexType::Chain>(vm, index, value); }

    ALWAYS_INLINE void deleteData(VM& vm, TableIndex index) { return set<IndexType::Data>(vm, index, vm.orderedHashTableDeletedValue()); }

    ALWAYS_INLINE void addToChain(VM& vm, TableIndex bucketIndex, TableIndex newChainStartKeyIndex)
    {
        JSValue prevChainStartKeyIndex = getChainStartKeyIndex(bucketIndex);
        setChainStartKeyIndex(bucketIndex, newChainStartKeyIndex);
        setChainData(vm, newChainStartKeyIndex + ChainOffset, prevChainStartKeyIndex);
    }

    ALWAYS_INLINE static TableIndex dataStartIndex(TableIndex dataTableStartIndex, Entry entry) { return dataTableStartIndex + entry * EntrySize; }

    ALWAYS_INLINE bool isValidChainIndex(TableIndex index) const
    {
        // ChainIndex = DataTableStartIndex + (Entry * EntrySize) + (EntrySize - 1)
        return isValidEntry((index - dataTableStartIndex() - (EntrySize - 1)) / EntrySize);
    }

    /* -------------------------------- OrderedHashTable -------------------------------- */
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

        TableIndex baseEntryKeyIndex = baseTable->dataTableStartIndex() - EntrySize;
        TableIndex baseDeletedEntriesIndex = baseTable->deletedEntriesStartIndex();

        TableIndex newEntryKeyIndex = newTable->dataTableStartIndex() - EntrySize;
        TableIndex newHashTableStartIndex = newTable->hashTableStartIndex();
        TableIndex newBucketCount = newTable->bucketCount();

        for (Entry baseEntry = 0; baseEntry < baseUsedCapacity; ++baseEntry) {
            baseEntryKeyIndex += EntrySize;
            JSValue baseKey = baseTable->getKeyOrValueData(baseEntryKeyIndex);
            ASSERT(!baseKey.isEmpty());

            // Step 1: Copy DataTable only for the alive entries.
            if (isDeleted(vm, baseKey)) {
                if constexpr (update == UpdateDeletedEntries::Yes)
                    baseTable->setDeletedEntry(baseDeletedEntriesIndex++, baseEntry);
                continue;
            }

            // Step 2: Copy the key and value from the base table to the new table.
            newEntryKeyIndex += EntrySize;
            newTable->setKeyOrValueData(vm, newEntryKeyIndex, baseKey);
            Traits::template copyValueDataIfNeeded(vm, baseTable, baseEntryKeyIndex + 1, newTable, newEntryKeyIndex + 1);

            // Step 3: Compute for the hash value and add to the chain in the new table. Note that the
            // key stored in the base table is already normalized.
            TableSize hash = jsMapHash(globalObject, vm, baseKey);
            RETURN_IF_EXCEPTION(scope, nullptr);
            TableIndex newBucketIndex = bucketIndex(newHashTableStartIndex, newBucketCount, hash);
            newTable->addToChain(vm, newBucketIndex, newEntryKeyIndex);
        }

        return newTable;
    }
    static Base* copy(JSGlobalObject* globalObject, Base* base)
    {
        Accessor* baseTable = static_cast<Accessor*>(base);
        Accessor* newTable = copyImpl<>(globalObject, baseTable, baseTable->capacity());
        return static_cast<Base*>(newTable);
    }

    ALWAYS_INLINE void updateOwnerStorage(VM& vm, HashTable* owner, Accessor* newTable)
    {
        ASSERT(owner->m_storage.get() == this && nextTable() == newTable);
        owner->m_storage.set(vm, owner, static_cast<Base*>(newTable));
    }

    enum class UpdateOwnerStorage : uint8_t {
        Yes,
        No
    };
    template<UpdateOwnerStorage update>
    ALWAYS_INLINE Accessor* rehash(JSGlobalObject* globalObject, HashTable* owner, TableSize newCapacity)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Rehash> Before: oldTable=", Dump(this, vm));

        Accessor* newTable = copyImpl<UpdateDeletedEntries::Yes>(globalObject, this, newCapacity);
        RETURN_IF_EXCEPTION(scope, { });

        setNextTable(vm, newTable);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Rehash> Rehashed: oldTable=", Dump(this, vm), "\nnewTable=", Dump(newTable, vm));

        if constexpr (update == UpdateOwnerStorage::Yes)
            updateOwnerStorage(vm, owner, newTable);
        return newTable;
    }

    ALWAYS_INLINE void clear(JSGlobalObject* globalObject, HashTable* owner)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Clear> Before: oldTable=", Dump(this, vm));

        Accessor* newTable = tryCreate(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        setClearedTableSentinel();
        setNextTable(vm, newTable);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Clear> Cleared: oldTable=", Dump(this, vm), "\nnewTable=", Dump(newTable, vm));

        owner->m_storage.set(vm, owner, static_cast<Base*>(newTable));
    }

    struct FindResult {
        JSValue normalizedKey;
        uint32_t hash;
        TableIndex bucketIndex;
        TableIndex entryKeyIndex;
    };
    ALWAYS_INLINE FindResult find(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());

        if (!aliveEntryCount()) {
            dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Find> No alive entries in table=", RawPointer(this));
            return { JSValue(), 0, InvalidTableIndex, InvalidTableIndex };
        }

        key = normalizeMapKey(key);
        TableSize hash = jsMapHash(globalObject, vm, key);
        RETURN_IF_EXCEPTION(scope, { });
        return find(globalObject, key, hash);
    }
    ALWAYS_INLINE FindResult find(JSGlobalObject* globalObject, JSValue normalizedKey, TableSize hash)
    {
        VM& vm = getVM(globalObject);
        TableIndex bucketIndex = this->bucketIndex(hash);
        ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);

        JSValue keyIndexValue = getChainStartKeyIndex(bucketIndex);
        while (!keyIndexValue.isEmpty()) {
            TableIndex entryKeyIndex = toNumber(keyIndexValue);
            JSValue entryKey = getKeyOrValueData(entryKeyIndex);
            // Fixme: Maybe we can compress the searching path by updating the chain with non-deleted entry.
            if (!isDeleted(vm, entryKey) && areKeysEqual(globalObject, normalizedKey, entryKey)) {
                dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Find> Found entry in the chain with bucketIndex=", bucketIndex, "  table=", RawPointer(this));
                return { normalizedKey, hash, bucketIndex, entryKeyIndex };
            }
            keyIndexValue = getChainData(entryKeyIndex + ChainOffset);
        }

        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Find> Not found the entry in the chain with bucketIndex=", bucketIndex, "  table=", RawPointer(this));
        return { normalizedKey, hash, bucketIndex, InvalidTableIndex };
    }

    ALWAYS_INLINE Accessor* expandIfNeeded(JSGlobalObject* globalObject, HashTable* owner)
    {
        ASSERT(!isObsolete());
        TableSize currentCapacity = capacity();
        if (usedCapacity() < currentCapacity)
            return this;

        TableSize newCapacity = currentCapacity << ExpandFactor;
        if (deletedEntryCount() >= (currentCapacity >> 1))
            newCapacity = currentCapacity; // No need to expanded. Just clear the deleted entries.
        return rehash<UpdateOwnerStorage::No>(globalObject, owner, newCapacity);
    }

    template<typename FindKeyFunctor>
    ALWAYS_INLINE void addImpl(JSGlobalObject* globalObject, HashTable* owner, JSValue key, JSValue value, const FindKeyFunctor& findKeyFunctor)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Add> Before: table=", Dump(this, vm));

        auto result = findKeyFunctor();
        RETURN_IF_EXCEPTION(scope, void());

        if (Accessor::isValidTableIndex(result.entryKeyIndex)) {
            Traits::template setValueDataIfNeeded(this, vm, result.entryKeyIndex + 1, value);
            dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Add> Found and set entryKeyIndex=", result.entryKeyIndex, " bucketIndex=", result.bucketIndex, " table=", Dump(this, vm));
            return;
        }

        Accessor* table = expandIfNeeded(globalObject, owner);
        RETURN_IF_EXCEPTION(scope, void());

        Entry newEntry = table->usedCapacity();
        TableIndex newEntryKeyIndex = dataStartIndex(table->dataTableStartIndex(), newEntry);
        table->incrementAliveEntryCount();

        bool firstAliveEntry = result.normalizedKey.isEmpty();
        if (UNLIKELY(firstAliveEntry)) {
            result.normalizedKey = normalizeMapKey(key);
            result.hash = jsMapHash(globalObject, vm, result.normalizedKey);
            RETURN_IF_EXCEPTION(scope, void());
        }

        bool rehashed = this != table;
        if (UNLIKELY(rehashed || firstAliveEntry))
            result.bucketIndex = table->bucketIndex(result.hash);

        ASSERT(table->isValidIndex<IndexType::Bucket>(result.bucketIndex) && normalizeMapKey(key) == result.normalizedKey);
        table->addToChain(vm, result.bucketIndex, newEntryKeyIndex);
        table->setKeyOrValueData(vm, newEntryKeyIndex, result.normalizedKey);
        Traits::template setValueDataIfNeeded(table, vm, newEntryKeyIndex + 1, value);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Add> Added newEntry=", newEntry, " entryKeyIndex=", result.entryKeyIndex, " bucketIndex=", result.bucketIndex, " table=", Dump(table, vm));

        if (UNLIKELY(rehashed))
            updateOwnerStorage(vm, owner, table);
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
        rehash<UpdateOwnerStorage::Yes>(globalObject, owner, currentCapacity >> 1);
    }

    template<typename FindKeyFunctor>
    ALWAYS_INLINE bool removeImpl(JSGlobalObject* globalObject, HashTable* owner, const FindKeyFunctor& findKeyFunctor)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        ASSERT(!isObsolete());

        auto result = findKeyFunctor();
        RETURN_IF_EXCEPTION(scope, false);

        if (!Accessor::isValidTableIndex(result.entryKeyIndex))
            return false;

        deleteData(vm, result.entryKeyIndex);
        Traits::template deleteValueDataIfNeeded(this, vm, result.entryKeyIndex + 1);
        incrementDeletedEntryCount();
        decrementAliveEntryCount();

        shrinkIfNeed(globalObject, owner);
        RETURN_IF_EXCEPTION(scope, false);
        dataLogLnIf(OrderedHashTableAccessorInternal::verbose, "<Remove> Removed entryKeyIndex=", result.entryKeyIndex, " bucketIndex=", result.bucketIndex, " table=", Dump(this, vm));
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
    struct TransitionResult {
        Accessor* table;
        Entry entry;
        JSValue key;
        JSValue value;
    };
    ALWAYS_INLINE TransitionResult transitAndNext(VM& vm, Entry from)
    {
        Accessor* table = transit([&](Accessor* obsoleteTale) ALWAYS_INLINE_LAMBDA {
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
            TableSize fromCopy = from;
            for (TableIndex i = deletedEntriesStartIndex; i < deletedEntriesEnd; ++i) {
                Entry deletedEntry = obsoleteTale->getDeletedEntry(i);
                if (deletedEntry >= fromCopy)
                    break;
                --from;
            }
        });

        ASSERT(table && !table->isObsolete());
        TableSize usedCapacity = table->usedCapacity();
        TableIndex entryKeyIndex = dataStartIndex(table->dataTableStartIndex(), from) - EntrySize;
        for (Entry entry = from; entry < usedCapacity; ++entry) {
            entryKeyIndex += EntrySize;
            JSValue key = table->getKeyOrValueData(entryKeyIndex);
            if (isDeleted(vm, key))
                continue;

            JSValue value = Traits::template getValueDataIfNeeded(table, entryKeyIndex + 1);
            return { table, entry, key, value };
        }
        return { };
    }

    ALWAYS_INLINE JSValue getKey(Entry entry)
    {
        ASSERT(!isObsolete());
        return getKeyOrValueData(dataStartIndex(dataTableStartIndex(), entry));
    }
    ALWAYS_INLINE JSValue getValue(Entry entry)
    {
        ASSERT(!isObsolete() && EntrySize == 3);
        return getKeyOrValueData(dataStartIndex(dataTableStartIndex(), entry) + 1);
    }
    ALWAYS_INLINE JSCell* nextAndUpdateIterationEntry(VM& vm, Entry entry)
    {
        auto result = transitAndNext(vm, entry);
        if (!result.table)
            return vm.orderedHashTableSentinel();
        updateIterationEntry(result.entry);
        return result.table;
    }
    ALWAYS_INLINE JSValue getIterationEntry() { return toJSValue(iterationEntry()); }
    ALWAYS_INLINE JSValue getIterationEntryKey() { return getKey(iterationEntry()); }
    ALWAYS_INLINE JSValue getIterationEntryValue() { return getValue(iterationEntry()); }

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
