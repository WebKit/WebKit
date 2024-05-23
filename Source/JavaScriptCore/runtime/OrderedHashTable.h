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
#include "Symbol.h"

namespace JSC {

struct MapTraits {
    static const uint8_t EntrySize = 2;

    template<typename Handle>
    static JSValue getValueData(Handle table, JSValue entry) { return table.getValueData(entry); }

    template<typename Handle>
    static void setValueData(Handle table, VM& vm, JSValue entry, JSValue value) { table.setValueData(vm, entry, value); }

    template<typename Handle>
    static void copyValueData(VM& vm, Handle baseTable, typename Handle::Entry baseEntry, Handle newTable, typename Handle::Entry newEntry)
    {
        JSValue baseValue = baseTable.getValueData(baseEntry);
        ASSERT(baseTable.isValidValueData(vm, baseValue));
        newTable.setValueData(vm, newEntry, baseValue);
    }

    template<typename Handle>
    static void deleteValueData(Handle table, VM& vm, JSValue entry) { table.deleteValueData(vm, entry); }
};

struct SetTraits {
    static const uint8_t EntrySize = 1;

    template<typename Handle>
    static JSValue getValueData(Handle table, JSValue entry) { return table.getKeyData(entry); }

    template<typename Handle>
    static void setValueData(Handle, VM&, JSValue, JSValue) { }

    template<typename Handle>
    static void copyValueData(VM&, Handle, typename Handle::Entry, Handle, typename Handle::Entry) { }

    template<typename Handle>
    static void deleteValueData(Handle, VM&, JSValue) { }
};

// Note that only ChainTable stores the real JSValues and the others are used as unsigned integer number wrapped in JSValue.
//
// ################ Non-Obsolete Table ################
//
//                      Count                                    Value(s)                                ValueType    WriteBarrier
//               ------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NumberOfAliveEntries                                            | TableSize | NO
//                         1          | NumberOfDeletedEntries                                          | TableSize | NO
//                         1          | Capacity                                                        | TableSize | NO
//                  NumberOfBuckets   | HashTable:  { <BucketIndex, ChainStartEntry>, ... }             | Entry     | NO
//                     Capacity       | ChainTable: { <Entry_0, NextEntry>, <Entry_1, NextEntry>, ... } | Entry     | NO
//  TableEnd  -> Capacity * EntrySize | DataTable:  { <Entry_0, Data_0>, <Entry_0 + 1, Data_1>, ... }   | JSValue   | Yes
//
// ################ Obsolete Table from Rehash ################
//
//                      Count                                    Value(s)                                ValueType    WriteBarrier
//               ------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                       |  TableSize | Yes
//                         1          | UsedCapacity                                                    |  TableSize | NO
//                         1          | Capacity                                                        |  TableSize | NO
//                    usedCapacity    | ObsoleteTable: { <Entry_1, NumberOfDeletedEntriesBefore>, ... } |  TableSize | NO
//  TableEnd  ->          ...         | NotUsed                                                         |  ...
//
// ################ Obsolete Table from Clear ################
//
//                      Count                                    Value(s)                                ValueType    WriteBarrier
//               ------------------------------------------------------------------------------------------------------------------
// TableStart ->           1          | NextTable                                                       |  TableSize | Yes
//                         1          | ClearedTableSentinel                                            |  TableSize | NO
//                         1          | Capacity                                                        |  TableSize | NO
//  TableEnd  ->          ...         | NotUsed                                                         |  ...
//
// The table is initialized with empty JSValues.
// 1. Found an empty entry while iterating the hash table means the current bucket is not taken by any chain.
// 2. Found an empty entry while iterating the chain table means the index entry is the end of the chain.
template<typename Traits>
class OrderedHashTable : public JSNonFinalObject {
    using Base = JSNonFinalObject;

public:
    using Storage = JSImmutableButterfly;

    DECLARE_VISIT_CHILDREN;

    /*  ------------------------ Handle START  ------------------------ */

    class Handle {
    public:
        using TableSize = uint32_t;
        using Entry = TableSize;
        using TableIndex = TableSize;

        TableSize ClearedTableSentinel = -1;
        static const uint8_t EntrySize = Traits::EntrySize;
        static const uint8_t LoadFactor = 2;
        static const uint8_t InitialCapacity = 4;

        static_assert(EntrySize == MapTraits::EntrySize || EntrySize == SetTraits::EntrySize);

        Handle() = default;

        Handle(Storage* butterfly)
            : m_storage(butterfly)
        {
        }

        explicit operator bool() const { return !!m_storage; }

        ALWAYS_INLINE static TableSize toNumber(JSValue number) { return number.asUInt32AsAnyInt(); }
        ALWAYS_INLINE static bool isDeleted(VM& vm, JSValue value) { return value.isCell() && value.asCell() == vm.orderedHashTableSentinel(); }
        ALWAYS_INLINE bool isValidEntry(Entry entry) const { return entry < usedCapacity(); }
        ALWAYS_INLINE bool isValidEntry(JSValue entry) const { return !entry.isEmpty() && entry.isNumber() && toNumber(entry) < usedCapacity(); }
        ALWAYS_INLINE static bool isValidValueData(VM& vm, JSValue value) { return !value.isEmpty() && !isDeleted(vm, value); }

        enum class IndexType : uint8_t {
            Any = 0,
            Default = 1,
            Bucket = 2,
            Chain = 3,
            Obsolete = 4,
            Data = 5,
        };

        template<IndexType type>
        bool isValidIndex(TableIndex index) const
        {
            switch (type) {
            case IndexType::Default:
                return numberOfAliveEntriesIndex() <= index && index <= capacityIndex();
            case IndexType::Bucket:
                return hashTableStartIndex() <= index && index <= hashTableEndIndex();
            case IndexType::Chain:
                return chainTableStartIndex() <= index && index <= chainTableEndIndex();
            case IndexType::Data:
                return dataTableStartIndex() <= index && index <= dataTableEndIndex();
            case IndexType::Obsolete:
                return obsoleteTableStartIndex() <= index && index <= obsoleteTableEndIndex();
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
        ALWAYS_INLINE void set(VM& vm, TableIndex index, JSValue value)
        {
            ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
            ASSERT_UNUSED(vm, (type <= IndexType::Obsolete && !isDeleted(vm, value)) || (type == IndexType::Data && isDeleted(vm, value)));
            return toButterfly()->contiguous().atUnsafe(index).setWithoutWriteBarrier(value);
        }
        template<IndexType type = IndexType::Any>
        ALWAYS_INLINE void set(VM& vm, TableIndex index, TableSize number) { set<type>(vm, index, JSValue(number)); }

        template<IndexType type = IndexType::Any>
        ALWAYS_INLINE void setWithWriteBarrier(VM& vm, TableIndex index, JSValue value)
        {
            ASSERT(!hasDouble(indexingType()) && isValidIndex<type>(index));
            ASSERT(type == IndexType::Data || index == numberOfAliveEntriesIndex());
            toButterfly()->contiguous().atUnsafe(index).set(vm, m_storage, value);
        }
        template<IndexType type = IndexType::Any>
        ALWAYS_INLINE void setWithWriteBarrier(VM& vm, TableIndex index, TableSize number) { setWithWriteBarrier(vm, m_storage, index, JSValue(number)); }

        /* -------------------------------- NumberOfAliveEntries, NumberOfDeletedEntries, and Capacity -------------------------------- */
        ALWAYS_INLINE static constexpr TableSize numberOfAliveEntriesIndex() { return 0; }
        ALWAYS_INLINE JSValue numberOfAliveEntriesSlot() const { return get<IndexType::Default>(numberOfAliveEntriesIndex()); }
        ALWAYS_INLINE TableSize numberOfAliveEntries() const { return toNumber(numberOfAliveEntriesSlot()); }
        ALWAYS_INLINE void setNumberOfAliveEntries(VM& vm, TableSize numberOfAliveEntries) { return set<IndexType::Default>(vm, numberOfAliveEntriesIndex(), numberOfAliveEntries); }
        ALWAYS_INLINE void incrementNumberOfAliveEntries(VM& vm)
        {
            TableSize number = numberOfAliveEntries();
            ASSERT(number < capacity());
            setNumberOfAliveEntries(vm, number + 1);
        }
        ALWAYS_INLINE void decrementNumberOfAliveEntries(VM& vm)
        {
            TableSize number = numberOfAliveEntries();
            ASSERT(number);
            setNumberOfAliveEntries(vm, number - 1);
        }

        ALWAYS_INLINE static constexpr TableSize numberOfDeletedEntriesIndex() { return numberOfAliveEntriesIndex() + 1; }
        ALWAYS_INLINE TableSize numberOfDeletedEntries() const { return toNumber(get<IndexType::Default>(numberOfDeletedEntriesIndex())); }
        ALWAYS_INLINE void setNumberOfDeletedEntries(VM& vm, TableSize numberOfDeletedEntries) { return set<IndexType::Default>(vm, numberOfDeletedEntriesIndex(), numberOfDeletedEntries); }
        ALWAYS_INLINE void incrementNumberOfDeletedEntries(VM& vm)
        {
            TableSize number = numberOfDeletedEntries();
            ASSERT(number < capacity());
            setNumberOfDeletedEntries(vm, number + 1);
        }

        ALWAYS_INLINE static constexpr TableSize capacityIndex() { return numberOfDeletedEntriesIndex() + 1; }
        ALWAYS_INLINE TableSize capacity() const { return toNumber(get<IndexType::Default>(capacityIndex())); }
        ALWAYS_INLINE void setCapacity(VM& vm, TableSize capacity) { set<IndexType::Default>(vm, capacityIndex(), capacity); }

        /* -------------------------------- Hash table -------------------------------- */
        ALWAYS_INLINE static constexpr TableSize numberOfBuckets(TableSize capacity) { return capacity / LoadFactor; }
        ALWAYS_INLINE TableSize numberOfBuckets() const { return numberOfBuckets(capacity()); }

        ALWAYS_INLINE static constexpr TableSize hashTableStartIndex() { return capacityIndex() + 1; }

        ALWAYS_INLINE static constexpr TableSize hashTableEndIndex(TableSize capacity) { return hashTableStartIndex() + numberOfBuckets(capacity) - 1; }
        ALWAYS_INLINE TableSize hashTableEndIndex() const { return hashTableEndIndex(capacity()); }

        ALWAYS_INLINE TableIndex bucketIndex(TableSize hash) { return hashTableStartIndex() + (hash & (numberOfBuckets() - 1)); }

        ALWAYS_INLINE JSValue getChainStartEntry(TableIndex index) { return get<IndexType::Bucket>(index); }
        ALWAYS_INLINE void setChainStartEntry(VM& vm, TableIndex index, JSValue entry)
        {
            ASSERT(isValidEntry(entry));
            return set<IndexType::Bucket>(vm, index, entry);
        }

        /* -------------------------------- Chain table -------------------------------- */
        ALWAYS_INLINE static TableSize chainTableStartIndex(TableSize capacity) { return hashTableEndIndex(capacity) + 1; }
        ALWAYS_INLINE TableSize chainTableStartIndex() const { return chainTableStartIndex(capacity()); }
        ALWAYS_INLINE static TableSize chainTableEndIndex(TableSize capacity) { return chainTableStartIndex(capacity) + capacity - 1; }
        ALWAYS_INLINE TableSize chainTableEndIndex() const { return chainTableEndIndex(capacity()); }

        ALWAYS_INLINE TableIndex chainIndex(JSValue entry)
        {
            ASSERT(isValidEntry(entry));
            return chainTableStartIndex() + toNumber(entry);
        }

        ALWAYS_INLINE JSValue getChainNextEntry(JSValue entry)
        {
            ASSERT(isValidEntry(entry));
            return get<IndexType::Chain>(chainIndex(entry));
        }
        ALWAYS_INLINE void setChainNextEntry(VM& vm, JSValue entry, JSValue nextEntry)
        {
            ASSERT(isValidEntry(entry));
            set<IndexType::Chain>(vm, chainIndex(entry), nextEntry);
        }

        ALWAYS_INLINE void addToChain(VM& vm, TableIndex index, JSValue newStartEntry)
        {
            JSValue prevStartEntry = getChainStartEntry(index);
            setChainStartEntry(vm, index, newStartEntry);
            setChainNextEntry(vm, newStartEntry, prevStartEntry);
        }
        ALWAYS_INLINE void addToChain(VM& vm, TableIndex index, Entry newStartEntry) { addToChain(vm, index, JSValue(newStartEntry)); }

        /* -------------------------------- Data table -------------------------------- */
        ALWAYS_INLINE static TableSize dataTableSize(TableSize capacity) { return capacity * EntrySize; }
        ALWAYS_INLINE TableSize dataTableSize() const { return dataTableSize(capacity()); }

        ALWAYS_INLINE static TableSize dataTableStartIndex(TableSize capacity) { return chainTableEndIndex(capacity) + 1; }
        ALWAYS_INLINE TableSize dataTableStartIndex() const { return dataTableStartIndex(capacity()); }

        ALWAYS_INLINE static TableSize dataTableEndIndex(TableSize capacity) { return dataTableStartIndex(capacity) + dataTableSize(capacity) - 1; }
        ALWAYS_INLINE TableSize dataTableEndIndex() const { return dataTableEndIndex(capacity()); }

        ALWAYS_INLINE JSValue getData(TableIndex index) { return get<IndexType::Data>(index); }
        ALWAYS_INLINE JSValue getKeyData(JSValue entry) { return getData(keyIndex(entry)); }
        ALWAYS_INLINE JSValue getKeyData(Entry entry) { return getData(keyIndex(entry)); }
        ALWAYS_INLINE JSValue getValueData(JSValue entry) { return getData(valueIndex(entry)); }
        ALWAYS_INLINE JSValue getValueData(Entry entry) { return getData(valueIndex(entry)); }

        ALWAYS_INLINE void setData(VM& vm, TableIndex index, JSValue value) { setWithWriteBarrier<IndexType::Data>(vm, index, value); }
        ALWAYS_INLINE void setKeyData(VM& vm, JSValue entry, JSValue value) { setData(vm, keyIndex(entry), value); }
        ALWAYS_INLINE void setKeyData(VM& vm, Entry entry, JSValue value) { setData(vm, keyIndex(entry), value); }
        ALWAYS_INLINE void setValueData(VM& vm, JSValue entry, JSValue value) { setData(vm, valueIndex(entry), value); }
        ALWAYS_INLINE void setValueData(VM& vm, Entry entry, JSValue value) { setData(vm, valueIndex(entry), value); }

        ALWAYS_INLINE void deleteData(VM& vm, TableIndex index) { return set<IndexType::Data>(vm, index, vm.orderedHashTableSentinel()); }
        ALWAYS_INLINE void deleteKeyData(VM& vm, JSValue entry) { return deleteData(vm, keyIndex(entry)); }
        ALWAYS_INLINE void deleteValueData(VM& vm, JSValue entry) { return deleteData(vm, valueIndex(entry)); }

        ALWAYS_INLINE TableIndex dataStartIndex(JSValue entry)
        {
            ASSERT(isValidEntry(entry));
            return dataTableStartIndex() + (toNumber(entry) << (EntrySize - 1));
        }

        ALWAYS_INLINE TableIndex keyIndex(JSValue entry) { return dataStartIndex(entry); }
        ALWAYS_INLINE TableIndex keyIndex(Entry entry) { return keyIndex(JSValue(entry)); }

        ALWAYS_INLINE TableIndex valueIndex(JSValue entry) { return dataStartIndex(entry) + 1; }
        ALWAYS_INLINE TableIndex valueIndex(Entry entry) { return valueIndex(JSValue(entry)); }

        /* -------------------------------- Overall table -------------------------------- */
        ALWAYS_INLINE static TableSize tableSize(TableSize capacity) { return dataTableEndIndex(capacity) + 1; }
        ALWAYS_INLINE static constexpr TableSize computeTableSize(TableSize capacity)
        {
            return 3 /* NumberOfAliveEntries, NumberOfDeletedEntries, and Capacity */
                + capacity / LoadFactor /* NumberOfBuckets */
                + capacity /* Capacity */
                + capacity * EntrySize; /* DataTableSize */
        }
        ALWAYS_INLINE TableSize tableSize() const
        {
            TableSize size = m_storage->length();
            ASSERT(size == tableSize(capacity()) && size == computeTableSize(capacity()));
            return size;
        }

        /* -------------------------------- Obsolete table -------------------------------- */
        ALWAYS_INLINE bool isObsolete() const { return !!nextTable(); }
        ALWAYS_INLINE Storage* nextTable() const
        {
            JSValue value = numberOfAliveEntriesSlot();
            if (value.isCell()) {
                ASSERT(jsDynamicCast<Storage*>(value));
                return jsCast<Storage*>(value);
            }
            return nullptr;
        }
        ALWAYS_INLINE void setNextTable(VM& vm, Handle next) { setWithWriteBarrier<IndexType::Default>(vm, numberOfAliveEntriesIndex(), next.storage()); }

        ALWAYS_INLINE TableSize usedCapacity() const
        {
            if (isObsolete())
                return numberOfDeletedEntries();
            return numberOfAliveEntries() + numberOfDeletedEntries();
        }
        ALWAYS_INLINE void setUsedCapacity(VM& vm, TableSize usedCapacity) { setNumberOfDeletedEntries(vm, usedCapacity); }

        ALWAYS_INLINE bool isCleared() const { return numberOfDeletedEntries() == ClearedTableSentinel; }
        ALWAYS_INLINE void setClearedTableSentinel(VM& vm) { setNumberOfDeletedEntries(vm, ClearedTableSentinel); }

        ALWAYS_INLINE TableIndex obsoleteTableStartIndex() const { return hashTableStartIndex(); }
        ALWAYS_INLINE TableIndex obsoleteTableEndIndex() const { return hashTableStartIndex() + usedCapacity() - 1; }
        ALWAYS_INLINE TableIndex obsoleteTableIndex(Entry entry) const
        {
            ASSERT(isValidEntry(entry));
            return hashTableStartIndex() + entry;
        }

        ALWAYS_INLINE TableSize numberOfDeletedEntriesBefore(Entry entry) const
        {
            ASSERT(isObsolete() && !isCleared());
            return entry ? toNumber(get<IndexType::Obsolete>(obsoleteTableIndex(entry - 1))) : 0;
        }
        ALWAYS_INLINE void setNumberOfDeletedEntriesBefore(VM& vm, Entry entry, TableSize number) { set<IndexType::Obsolete>(vm, obsoleteTableIndex(entry), number); }

        /* ------------------------------------------------------------------------------------ */

        ALWAYS_INLINE static Storage* tryCreate(JSGlobalObject* globalObject, TableSize numberOfAliveEntries = 0, TableSize numberOfDeletedEntries = 0, TableSize capacity = InitialCapacity)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!(capacity & (capacity - 1)));

            TableSize length = tableSize(capacity);
            if (UNLIKELY(length > IndexingHeader::maximumLength)) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            Storage* butterfly = Storage::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), length);
            if (UNLIKELY(!butterfly)) {
                throwOutOfMemoryError(globalObject, scope);
                return nullptr;
            }

            Handle table(butterfly);
            table.setNumberOfAliveEntries(vm, numberOfAliveEntries);
            table.setNumberOfDeletedEntries(vm, numberOfDeletedEntries);
            table.setCapacity(vm, capacity);
            return butterfly;
        }

        ALWAYS_INLINE static Handle copyImpl(JSGlobalObject* globalObject, Handle base, TableSize newCapacity)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);

            TableSize baseCapacity = base.capacity();
            TableSize baseUsedCapacity = base.usedCapacity();
            TableSize baseNumberOfAliveEntries = base.numberOfAliveEntries();
            ASSERT(base && !base.isObsolete());
            ASSERT_UNUSED(baseNumberOfAliveEntries, newCapacity >= InitialCapacity && newCapacity >= baseNumberOfAliveEntries);
            ASSERT_UNUSED(baseCapacity, baseUsedCapacity <= baseCapacity);

            Handle newTable = tryCreate(globalObject, base.numberOfAliveEntries(), 0, newCapacity);
            RETURN_IF_EXCEPTION(scope, nullptr);

            TableSize newEntry = 0;
            for (Entry baseEntry = 0; baseEntry < baseUsedCapacity; ++baseEntry) {
                JSValue baseKey = base.getKeyData(baseEntry);
                ASSERT(!baseKey.isEmpty());

                // Step 1: Copy DataTable only for the alive entries.
                if (isDeleted(vm, baseKey))
                    continue;

                // Step 2: Compute for the hash value and add to the chain. Note that the
                // key stored in the base table is already normalized.
                TableSize hash = jsMapHash(globalObject, vm, baseKey);
                RETURN_IF_EXCEPTION(scope, nullptr);
                TableIndex bucketIndex = newTable.bucketIndex(hash);
                newTable.addToChain(vm, bucketIndex, newEntry);

                // Step 3: Copy the base entries to the new table.
                newTable.setKeyData(vm, newEntry, baseKey);
                Traits::template copyValueData(vm, base, baseEntry, newTable, newEntry);

                ++newEntry;
            }

            return newTable.storage();
        }

        static Storage* copy(JSGlobalObject* globalObject, Handle base) { return copyImpl(globalObject, base, base.capacity()).storage(); }

        Handle rehash(JSGlobalObject* globalObject, TableSize newCapacity)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Rehash> Before: oldTable=", Dump(*this, vm));

            // Step 1: Copy the data from the old table to the new table except for the deleted entries.
            Handle newTable = copyImpl(globalObject, m_storage, newCapacity);
            RETURN_IF_EXCEPTION(scope, { });

            // Step 2: Update the ObsoleteTable which starts from entry_1 not entry_0.
            // Since entry_0 always have zero deleted entries before.
            TableSize numberOfDeletedEntries = 0;
            TableSize oldUsedCapacity = usedCapacity();
            for (Entry oldEntry = 0; oldEntry <= oldUsedCapacity; ++oldEntry) {
                if (oldEntry)
                    setNumberOfDeletedEntriesBefore(vm, oldEntry - 1, numberOfDeletedEntries);

                if (oldEntry < oldUsedCapacity) {
                    JSValue oldKey = getKeyData(oldEntry);
                    ASSERT(!oldKey.isEmpty());
                    if (isDeleted(vm, oldKey))
                        ++numberOfDeletedEntries;
                }
            }

            // Step 3: Set the used capacity.
            setUsedCapacity(vm, oldUsedCapacity);

            // Step 4: Set the NextTable.
            setNextTable(vm, newTable);

            dataLogLnIf(Options::dumpOrderedHashTable(), "<Rehash> Rehashed: oldTable=", Dump(*this, vm), "\nnewTable=", Dump(newTable, vm));
            return newTable;
        }

        void clear(JSGlobalObject* globalObject)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Clear> Before: oldTable=", Dump(*this, vm));

            // Step 1: Create a new table.
            Handle newTable = tryCreate(globalObject);
            RETURN_IF_EXCEPTION(scope, void());

            // Step 2: Set the ClearedTableSentinel.
            setClearedTableSentinel(vm);

            // Step 3: Set NextTable.
            setNextTable(vm, newTable);

            dataLogLnIf(Options::dumpOrderedHashTable(), "<Clear> Cleared and the newTable=", Dump(newTable, vm));
        }

        // Return entry, normalizedKey, bucketIndex
        std::tuple<JSValue, JSValue, JSValue> find(JSGlobalObject* globalObject, JSValue key)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete());

            if (!numberOfAliveEntries()) {
                dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> No entries in table=", RawPointer(this->m_storage));
                return { };
            }

            key = normalizeMapKey(key);
            TableSize hash = jsMapHash(globalObject, vm, key);
            RETURN_IF_EXCEPTION(scope, { });
            return find(globalObject, key, hash);
        }

        // Return entry, normalizedKey, bucketIndex
        std::tuple<JSValue, JSValue, JSValue> find(JSGlobalObject* globalObject, JSValue normalizedKey, TableSize hash)
        {
            ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);
            VM& vm = getVM(globalObject);
            TableIndex index = bucketIndex(hash);

            JSValue entry = getChainStartEntry(index);
            while (!entry.isEmpty()) {
                JSValue entryKey = getKeyData(entry);
                if (!isDeleted(vm, entryKey) && areKeysEqual(globalObject, normalizedKey, entryKey)) {
                    dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> Found entry=", entry, " in the chain with bucketIndex=", index, "  table=", RawPointer(this->m_storage));
                    return { entry, normalizedKey, JSValue(index) };
                }
                entry = getChainNextEntry(entry);
            }

            dataLogLnIf(Options::dumpOrderedHashTable(), "<Find> Not found the entry in the chain with bucketIndex=", index, "  table=", RawPointer(this->m_storage));
            return { JSValue(), normalizedKey, JSValue(index) };
        }

        Handle expandIfNeeded(JSGlobalObject* globalObject)
        {
            ASSERT(!isObsolete());
            TableSize currentCapacity = capacity();
            if (usedCapacity() < currentCapacity)
                return m_storage;

            TableSize newCapacity = currentCapacity << 1;
            if (numberOfDeletedEntries() >= (currentCapacity >> 1))
                newCapacity = currentCapacity; // No need to expanded. Just clear the deleted entries.
            return rehash(globalObject, newCapacity);
        }

        void addNew(VM& vm, TableIndex bucketIndex, JSValue entry, JSValue key, JSValue value = JSValue())
        {
            ASSERT(!isObsolete() && isValidIndex<IndexType::Bucket>(bucketIndex) && isValidEntry(entry) && !key.isEmpty());
            addToChain(vm, bucketIndex, entry);
            setKeyData(vm, entry, key);
            Traits::template setValueData<Handle>(*this, vm, entry, value);
        }

        void add(JSGlobalObject* globalObject, JSValue key, JSValue value = JSValue())
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete());

            Handle table = expandIfNeeded(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Before: table=", Dump(table, vm));

            auto [entry, normalizedKey, bucketIndex] = table.find(globalObject, key);
            RETURN_IF_EXCEPTION(scope, void());

            if (!entry.isEmpty()) {
                Traits::template setValueData<Handle>(table, vm, entry, value);
                dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Found and set entry=", entry, " at bucketIndex=", toNumber(bucketIndex), " table=", Dump(table, vm));
            } else {
                entry = JSValue(table.usedCapacity());
                table.incrementNumberOfAliveEntries(vm); // TODO: should update in place

                if (bucketIndex.isEmpty()) {
                    // This is the first entry in the table.
                    ASSERT(normalizedKey.isEmpty());
                    normalizedKey = normalizeMapKey(key);
                    TableSize hash = jsMapHash(globalObject, vm, normalizedKey);
                    RETURN_IF_EXCEPTION(scope, void());
                    bucketIndex = JSValue(table.bucketIndex(hash));
                }

                ASSERT(normalizeMapKey(key) == normalizedKey);
                table.addNew(vm, toNumber(bucketIndex), entry, normalizedKey, value);
                dataLogLnIf(Options::dumpOrderedHashTable(), "<Add> Added entry=", entry, " at bucketIndex=", toNumber(bucketIndex), " table=", Dump(table, vm));
            }
        }

        JSArray* addNormalized(JSGlobalObject* globalObject, JSValue normalizedKey, JSValue value, TableSize hash)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);

            Handle table = expandIfNeeded(globalObject);
            RETURN_IF_EXCEPTION(scope, { });
            dataLogLnIf(Options::dumpOrderedHashTable(), "<AddNormalized> Before: table=", Dump(table, vm));

            auto [entry, _, bucketIndex] = table.find(globalObject, normalizedKey, hash);
            RETURN_IF_EXCEPTION(scope, { });

            if (!entry.isEmpty()) {
                Traits::template setValueData<Handle>(table, vm, entry, value);
                dataLogLnIf(Options::dumpOrderedHashTable(), "<AddNormalized> Found and set entry=", entry, " at bucketIndex=", toNumber(bucketIndex), " table=", Dump(table, vm));
            } else {
                entry = JSValue(table.usedCapacity());
                table.incrementNumberOfAliveEntries(vm);

                table.addNew(vm, toNumber(bucketIndex), entry, normalizedKey, value);
                dataLogLnIf(Options::dumpOrderedHashTable(), "<AddNormalized> Added entry=", entry, "  table=", Dump(table, vm));
            }
            RELEASE_AND_RETURN(scope, createTuple(globalObject, normalizedKey, value));
        }

        Handle shrinkIfNeed(JSGlobalObject* globalObject)
        {
            ASSERT(!isObsolete());
            TableSize currentCapacity = capacity();
            if (numberOfAliveEntries() >= (currentCapacity >> 2))
                return m_storage;
            if (currentCapacity == InitialCapacity)
                return m_storage;
            return rehash(globalObject, currentCapacity >> 1);
        }

        Handle removeExisting(JSGlobalObject* globalObject, JSValue entry)
        {
            ASSERT(!isObsolete() && isValidEntry(entry));
            VM& vm = getVM(globalObject);
            deleteKeyData(vm, entry);
            Traits::template deleteValueData(*this, vm, entry);
            incrementNumberOfDeletedEntries(vm);
            decrementNumberOfAliveEntries(vm);
            return shrinkIfNeed(globalObject);
        }

        bool remove(JSGlobalObject* globalObject, JSValue key)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete());

            auto [entry, _, bucketIndex] = find(globalObject, key);
            RETURN_IF_EXCEPTION(scope, false);

            if (entry.isEmpty())
                return false;

            Handle table = removeExisting(globalObject, entry);
            RETURN_IF_EXCEPTION(scope, false);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<Remove> Removed entry=", entry, " at bucketIndex=", toNumber(bucketIndex), " table=", Dump(table, vm));
            return true;
        }

        bool removeNormalized(JSGlobalObject* globalObject, JSValue normalizedKey, TableSize hash)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete() && normalizeMapKey(normalizedKey) == normalizedKey);

            auto [entry, _, bucketIndex] = find(globalObject, normalizedKey, hash);
            RETURN_IF_EXCEPTION(scope, false);

            if (entry.isEmpty())
                return false;

            Handle table = removeExisting(globalObject, entry);
            RETURN_IF_EXCEPTION(scope, false);
            dataLogLnIf(Options::dumpOrderedHashTable(), "<RemoveNormalized> Removed entry=", entry, " at bucketIndex=", toNumber(bucketIndex), " table=", Dump(table, vm));
            return true;
        }

        bool has(JSGlobalObject* globalObject, JSValue key)
        {
            VM& vm = getVM(globalObject);
            auto scope = DECLARE_THROW_SCOPE(vm);
            ASSERT(!isObsolete());

            JSValue entry = std::get<0>(find(globalObject, key));
            RETURN_IF_EXCEPTION(scope, false);
            return !entry.isEmpty();
        }

        template<typename Functor>
        Storage* transitOrNull(const Functor& functor)
        {
            if (!m_storage)
                return nullptr;

            Handle table(m_storage);
            while (table.isObsolete()) {
                functor(table);
                table = table.nextTable();
            }
            return table.storage();
        }
        Storage* transitOrNull()
        {
            return transitOrNull([](Handle) { });
        }

        JSValue getValueDataOrEmpty(VM& vm, JSValue entry)
        {
            ASSERT(!isObsolete());
            if (entry.isEmpty())
                return JSValue();

            ASSERT(toNumber(entry) < usedCapacity());
            JSValue value = Traits::template getValueData(*this, entry);
            ASSERT_UNUSED(vm, isValidValueData(vm, value));
            return value;
        }

        std::tuple<JSValue, JSValue, JSValue> getKeyValueEntry(VM& vm, Entry from)
        {
            ASSERT(!isObsolete());
            TableSize currentUsedCapacity = usedCapacity();
            for (Entry entry = from; entry < currentUsedCapacity; ++entry) {
                JSValue key = getKeyData(entry);
                // The key may be an empty JSValue if the map was previously cleared.
                if (key.isEmpty())
                    break;
                if (isDeleted(vm, key))
                    continue;
                JSValue value = Traits::template getValueData(*this, JSValue(entry));
                ASSERT(isValidValueData(vm, value));
                return { key, value, JSValue(entry) };
            }
            return { };
        }

        // This should be only used for iteration. And it should be guarded by the storage sentinel check.
        std::tuple<Handle, JSValue, JSValue, JSValue> getTableKeyValueEntry(VM& vm, Entry from)
        {
            ASSERT(m_storage);
            Handle newTable = transitOrNull([&](Handle obsoleteTable) {
                if (obsoleteTable.isCleared())
                    from = 0;
                else {
                    TableSize number = obsoleteTable.numberOfDeletedEntriesBefore(from);
                    ASSERT(from >= number);
                    from -= number;
                }
            });

            auto [nextKey, nextValue, nextEntry] = newTable.getKeyValueEntry(vm, from);
            return { newTable, nextKey, nextValue, nextEntry };
        }

        Storage* storage() const { return m_storage; }

        class Dump {
        public:
            Dump(Handle table, VM& vm)
                : m_storage(table)
                , m_vm(vm)
            {
            }

            void dump(PrintStream& out) const;

            Handle m_storage;
            VM& m_vm;
        };

    private:
        IndexingType indexingType() const { return m_storage->indexingType(); }
        Butterfly* toButterfly() const { return m_storage->toButterfly(); }
        Storage* m_storage;
    };

    /*  ------------------------ Handle END  ------------------------ */

    OrderedHashTable(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    static ptrdiff_t offsetOfTable() { return OBJECT_OFFSETOF(OrderedHashTable, m_storage); }

    void finishCreation(VM& vm) { Base::finishCreation(vm); }
    void finishCreation(JSGlobalObject* globalObject, VM& vm, OrderedHashTable<Traits>* base)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        Base::finishCreation(vm);
        RELEASE_AND_RETURN(scope, clone(globalObject, base));
    }

    JSValue getOrEmpty(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle table = transitOrNull(globalObject)) {
            JSValue entry = std::get<0>(table.find(globalObject, key));
            RETURN_IF_EXCEPTION(scope, { });
            return table.getValueDataOrEmpty(vm, entry);
        }
        return { };
    }

    JSValue getOrEmpty(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle table = transitOrNull(globalObject)) {
            JSValue entry = std::get<0>(table.find(globalObject, key, hash));
            RETURN_IF_EXCEPTION(scope, { });
            return table.getValueDataOrEmpty(vm, entry);
        }
        return { };
    }

    bool has(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle table = transitOrNull(globalObject)) {
            JSValue entry = std::get<0>(table.find(globalObject, key));
            RETURN_IF_EXCEPTION(scope, { });
            return !entry.isEmpty();
        }
        return false;
    }

    void add(JSGlobalObject* globalObject, JSValue key, JSValue value = { })
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!m_storage) {
            Storage* storage = Handle::tryCreate(globalObject);
            RETURN_IF_EXCEPTION(scope, void());
            m_storage.set(getVM(globalObject), this, storage);
        }

        Handle table = transitOrNull(globalObject);
        ASSERT(table);
        RELEASE_AND_RETURN(scope, table.add(globalObject, key, value));
    }

    JSArray* addNormalized(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (!m_storage) {
            Storage* storage = Handle::tryCreate(globalObject);
            RETURN_IF_EXCEPTION(scope, nullptr);
            m_storage.set(getVM(globalObject), this, storage);
        }

        Handle table = transitOrNull(globalObject);
        ASSERT(table);
        RELEASE_AND_RETURN(scope, table.addNormalized(globalObject, key, value, hash));
    }

    bool remove(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle table = transitOrNull(globalObject))
            RELEASE_AND_RETURN(scope, table.remove(globalObject, key));
        return false;
    }

    bool removeNormalized(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle table = transitOrNull(globalObject))
            RELEASE_AND_RETURN(scope, table.removeNormalized(globalObject, key, hash));
        return false;
    }

    void clone(JSGlobalObject* globalObject, OrderedHashTable<Traits>* base)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Handle baseTable = base->transitOrNull(globalObject)) {
            Storage* storage = Handle::copy(globalObject, baseTable);
            RETURN_IF_EXCEPTION(scope, void());
            m_storage.set(vm, this, storage);
        }
    }

    void clear(JSGlobalObject* globalObject)
    {
        if (Handle table = transitOrNull(globalObject))
            table.clear(globalObject);
    }

    JSCell* transitOrSentinel(VM& vm)
    {
        Handle table = m_storage.get();
        if (Storage* storage = table.transitOrNull())
            return storage;
        return vm.orderedHashTableSentinel();
    }

    uint32_t size(JSGlobalObject* globalObject)
    {
        if (Handle table = transitOrNull(globalObject))
            return table.numberOfAliveEntries();
        return 0;
    }

private:
    Storage* transitOrNull(JSGlobalObject* globalObject)
    {
        Handle table = m_storage.get();
        Storage* newStorage = table.transitOrNull();
        if (newStorage && newStorage != m_storage.get())
            m_storage.set(getVM(globalObject), this, newStorage);
        return newStorage;
    }

    WriteBarrier<Storage> m_storage;
};

class OrderedHashMap : public OrderedHashTable<MapTraits> {
    using Base = OrderedHashTable<MapTraits>;

public:
    OrderedHashMap(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JSValue get(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSValue result = getOrEmpty(globalObject, key);
        RETURN_IF_EXCEPTION(scope, { });
        return result.isEmpty() ? jsUndefined() : result;
    }

    static Symbol* createSentinel(VM& vm) { return Symbol::create(vm); }
};

class OrderedHashSet : public OrderedHashTable<SetTraits> {
    using Base = OrderedHashTable<SetTraits>;

public:
    OrderedHashSet(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }
};

template<typename Traits>
void OrderedHashTable<Traits>::Handle::Dump::dump(PrintStream& out) const
{
    static bool breakIfEmpty = false;

    if (m_storage.isObsolete()) {
        out.print("\nthis=", RawPointer(m_storage.m_storage), "\n");
        out.print("NextTable=", RawPointer(m_storage.nextTable()), "\n");
        out.print(m_storage.isCleared() ? "ClearedTableSentinel=" : "UsedCapacity=", m_storage.numberOfDeletedEntries(), "\n");
        out.print("Capacity=", m_storage.capacity(), "\n");

        if (!m_storage.isCleared()) {
            out.print("ObsoleteTable=[");
            for (TableIndex i = m_storage.obsoleteTableStartIndex(); i <= m_storage.obsoleteTableEndIndex(); ++i) {
                JSValue entry = m_storage.get<IndexType::Obsolete>(i);
                if (entry.isEmpty()) {
                    if (breakIfEmpty)
                        break;
                    out.print("EMPTY");
                } else
                    out.print(m_storage.toNumber(entry));
                if (i != m_storage.obsoleteTableEndIndex())
                    out.print(", ");
            }
            ASSERT(m_storage.usedCapacity() == m_storage.obsoleteTableEndIndex() - m_storage.obsoleteTableStartIndex() + 1);
            out.print("] ObsoleteTableSize=", m_storage.usedCapacity(), "\n");
        }
        return;
    }

    out.print("\nthis=", RawPointer(m_storage.m_storage), "\n");
    out.print("NumberOfAliveEntries=", m_storage.numberOfAliveEntries(), "\n");
    out.print("NumberOfDeletedEntries=", m_storage.numberOfDeletedEntries(), "\n");
    out.print("Capacity=", m_storage.capacity(), "\n");

    out.print("HashTable=[");
    for (TableIndex i = m_storage.hashTableStartIndex(); i <= m_storage.hashTableEndIndex(); ++i) {
        JSValue entry = m_storage.get<IndexType::Bucket>(i);
        if (entry.isEmpty()) {
            if (breakIfEmpty)
                break;
            out.print("EMPTY");
        } else
            out.print(m_storage.toNumber(entry));
        if (i != m_storage.hashTableEndIndex())
            out.print(", ");
    }
    ASSERT(m_storage.numberOfBuckets() == m_storage.hashTableEndIndex() - m_storage.hashTableStartIndex() + 1);
    out.print("] HashTableSize=", m_storage.numberOfBuckets(), "\n");

    out.print("ChainTable=[");
    for (TableIndex i = m_storage.chainTableStartIndex(); i <= m_storage.chainTableEndIndex(); ++i) {
        JSValue entry = m_storage.get<IndexType::Chain>(i);
        if (entry.isEmpty()) {
            if (breakIfEmpty)
                break;
            out.print("EMPTY");
        } else
            out.print(toNumber(entry));
        if (i != m_storage.chainTableEndIndex())
            out.print(", ");
    }
    ASSERT(m_storage.capacity() == m_storage.chainTableEndIndex() - m_storage.chainTableStartIndex() + 1);
    out.print("] ChainTableSize=", m_storage.capacity(), "\n");

    out.print("DataTable=[\n");
    for (TableIndex i = m_storage.dataTableStartIndex(); i <= m_storage.dataTableEndIndex(); ++i) {
        JSValue value = m_storage.get<IndexType::Data>(i);
        out.print("    ");
        if (value.isEmpty()) {
            if (breakIfEmpty)
                break;
            out.print("EMPTY");
        } else if (isDeleted(m_vm, value))
            out.print("DELETED");
        else
            out.print(value);
        out.print(",\n");
    }
    ASSERT(m_storage.dataTableSize() == m_storage.dataTableEndIndex() - m_storage.dataTableStartIndex() + 1);
    out.print("] DataTableSize=", m_storage.dataTableSize(), "\n");
}

} // namespace JSC
