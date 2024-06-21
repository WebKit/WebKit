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

#include "OrderedHashTableAccessor.h"

namespace JSC {

template<typename Traits>
class OrderedHashTable : public JSNonFinalObject {
    using Base = JSNonFinalObject;

public:
    using HashTable = OrderedHashTable<Traits>;
    using Accessor = OrderedHashTableAccessor<Traits>;
    using Storage = JSImmutableButterfly;
    using TableIndex = typename Accessor::TableIndex;

    DECLARE_VISIT_CHILDREN;

    OrderedHashTable(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    static ptrdiff_t offsetOfButterfly() { return OBJECT_OFFSETOF(OrderedHashTable, m_storage); }

    void finishCreation(VM& vm) { Base::finishCreation(vm); }
    void finishCreation(JSGlobalObject* globalObject, VM& vm, OrderedHashTable<Traits>* base)
    {
        auto scope = DECLARE_THROW_SCOPE(vm);
        Base::finishCreation(vm);
        RELEASE_AND_RETURN(scope, clone(globalObject, base));
    }

    ALWAYS_INLINE TableIndex getKeyIndex(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Accessor* table = static_cast<Accessor*>(m_storage.get())) {
            auto result = table->find(globalObject, key, hash);
            RELEASE_AND_RETURN(scope, result.entryKeyIndex);
        }
        return Accessor::InvalidTableIndex;
    }

    ALWAYS_INLINE bool has(JSGlobalObject* globalObject, JSValue key)
    {
        if (Accessor* table = static_cast<Accessor*>(m_storage.get())) {
            auto result = table->find(globalObject, key);
            return result.entryKeyIndex != Accessor::InvalidTableIndex;
        }
        return false;
    }
    ALWAYS_INLINE bool has(JSGlobalObject* globalObject, JSValue normalizedKey, uint32_t hash)
    {
        if (Accessor* table = static_cast<Accessor*>(m_storage.get())) {
            auto result = table->find(globalObject, normalizedKey, hash);
            return result.entryKeyIndex != Accessor::InvalidTableIndex;
        }
        return false;
    }

    ALWAYS_INLINE void add(JSGlobalObject* globalObject, JSValue key, JSValue value = { })
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        Accessor* table = materializeIfNeeded(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        RELEASE_AND_RETURN(scope, table->add(globalObject, this, key, value));
    }
    ALWAYS_INLINE void addNormalized(JSGlobalObject* globalObject, JSValue key, JSValue value, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        Accessor* table = materializeIfNeeded(globalObject);
        RETURN_IF_EXCEPTION(scope, void());

        RELEASE_AND_RETURN(scope, table->addNormalized(globalObject, this, key, value, hash));
    }

    ALWAYS_INLINE bool remove(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Accessor* table = static_cast<Accessor*>(m_storage.get()))
            RELEASE_AND_RETURN(scope, table->remove(globalObject, this, key));
        return false;
    }
    ALWAYS_INLINE bool removeNormalized(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Accessor* table = static_cast<Accessor*>(m_storage.get()))
            RELEASE_AND_RETURN(scope, table->removeNormalized(globalObject, this, key, hash));
        return false;
    }

    void clone(JSGlobalObject* globalObject, OrderedHashTable<Traits>* base)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Accessor* table = static_cast<Accessor*>(base->m_storage.get())) {
            Storage* storage = Accessor::copy(globalObject, table);
            RETURN_IF_EXCEPTION(scope, void());
            m_storage.set(vm, this, storage);
        }
    }

    uint32_t size()
    {
        if (Accessor* table = static_cast<Accessor*>(m_storage.get()))
            return table->aliveEntryCount();
        return 0;
    }

    ALWAYS_INLINE void clear(JSGlobalObject* globalObject)
    {
        if (Accessor* table = static_cast<Accessor*>(m_storage.get()))
            table->clear(globalObject, this);
    }

    ALWAYS_INLINE Accessor* materializeIfNeeded(JSGlobalObject* globalObject)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (m_storage)
            return static_cast<Accessor*>(m_storage.get());

        Accessor* newTable = Accessor::tryCreate(globalObject);
        Storage* storage = static_cast<Storage*>(newTable);
        RETURN_IF_EXCEPTION(scope, nullptr);

        m_storage.set(vm, this, storage);
        return newTable;
    }

    JSCell* storageOrSentinel(VM& vm)
    {
        if (m_storage)
            return m_storage.get();
        return vm.orderedHashTableSentinel();
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

    template<typename FindKeyFunctor>
    ALWAYS_INLINE JSValue getImpl(JSGlobalObject* globalObject, const FindKeyFunctor& findKeyFunctor)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);

        if (Accessor* table = static_cast<Accessor*>(m_storage.get())) {
            auto result = findKeyFunctor(table);
            RETURN_IF_EXCEPTION(scope, { });

            if (!Accessor::isValidTableIndex(result.entryKeyIndex))
                return { };

            JSValue value = MapTraits::template getValueData(table, result.entryKeyIndex);
            ASSERT(table->isValidValueData(vm, value));
            return value;
        }
        return { };
    }
    JSValue get(JSGlobalObject* globalObject, JSValue key)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSValue result = getImpl(globalObject, [&](Accessor* table) ALWAYS_INLINE_LAMBDA {
            return table->find(globalObject, key);
        });
        RETURN_IF_EXCEPTION(scope, { });
        return result.isEmpty() ? jsUndefined() : result;
    }
    JSValue get(JSGlobalObject* globalObject, JSValue key, uint32_t hash)
    {
        VM& vm = getVM(globalObject);
        auto scope = DECLARE_THROW_SCOPE(vm);
        JSValue result = getImpl(globalObject, [&](Accessor* table) ALWAYS_INLINE_LAMBDA {
            return table->find(globalObject, key, hash);
        });
        RETURN_IF_EXCEPTION(scope, { });
        return result.isEmpty() ? jsUndefined() : result;
    }
    JSValue get(TableIndex keyIndex)
    {
        ASSERT(m_storage);
        Accessor* table = static_cast<Accessor*>(m_storage.get());
        return table->getKeyOrValueData(keyIndex + 1);
    }

    static JSCell* createSentinel(VM& vm) { return Accessor::tryCreate(vm, 0); }
    static Symbol* createDeletedValue(VM& vm) { return Symbol::create(vm); }
};

class OrderedHashSet : public OrderedHashTable<SetTraits> {
    using Base = OrderedHashTable<SetTraits>;

public:
    OrderedHashSet(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }
};

} // namespace JSC
