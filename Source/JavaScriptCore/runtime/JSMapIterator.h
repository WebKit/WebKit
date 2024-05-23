/*
 * Copyright (C) 2013-2024 Apple, Inc. All rights reserved.
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

#include "IterationKind.h"
#include "JSInternalFieldObjectImpl.h"
#include "JSMap.h"

namespace JSC {

const static uint8_t JSMapIteratorNumberOFInternalFields = 4;

class JSMapIterator final : public JSInternalFieldObjectImpl<JSMapIteratorNumberOFInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSMapIteratorNumberOFInternalFields>;

    DECLARE_EXPORT_INFO;

    enum class Field : uint8_t {
        Entry = 0,
        IteratedObject,
        Storage,
        Kind,
    };
    static_assert(numberOfInternalFields == JSMapIteratorNumberOFInternalFields);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNumber(0),
            jsNull(),
            jsNull(),
            jsNumber(0),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.mapIteratorSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSMapIterator* create(JSGlobalObject* globalObject, Structure* structure, JSMap* iteratedObject, IterationKind kind)
    {
        VM& vm = getVM(globalObject);
        JSMapIterator* instance = new (NotNull, allocateCell<JSMapIterator>(vm)) JSMapIterator(vm, structure);
        instance->finishCreation(globalObject, iteratedObject, kind);
        return instance;
    }

    static JSMapIterator* createWithInitialValues(VM&, Structure*);

    std::tuple<JSValue, JSValue> nextWithAdvance(VM& vm, uint32_t entry)
    {
        JSCell* storageCell = storage();
        if (storageCell == vm.orderedHashTableSentinel())
            return { };

        JSMap::Handle table = jsCast<JSMap::Storage*>(storageCell);
        auto [newTable, nextKey, nextValue, nextEntry] = table.getTableKeyValueEntry(vm, entry);
        if (nextKey.isEmpty()) {
            setStorage(vm, vm.orderedHashTableSentinel());
            return { };
        }

        setEntry(vm, nextEntry.asUInt32() + 1);
        if (newTable.storage() != table.storage())
            setStorage(vm, newTable.storage());
        return { nextKey, nextValue };
    }

    bool next(JSGlobalObject* globalObject, JSValue& value)
    {
        auto [nextKey, nextValue] = nextWithAdvance(globalObject->vm(), entry());
        if (nextKey.isEmpty())
            return false;

        switch (kind()) {
        case IterationKind::Values:
            value = nextValue;
            break;
        case IterationKind::Keys:
            value = nextKey;
            break;
        case IterationKind::Entries:
            value = createTuple(globalObject, nextKey, nextValue);
            break;
        }
        return true;
    }

    bool nextKeyValue(JSGlobalObject* globalObject, JSValue& key, JSValue& value)
    {
        auto [nextKey, nextValue] = nextWithAdvance(globalObject->vm(), entry());
        if (nextKey.isEmpty())
            return false;

        key = nextKey;
        value = nextValue;
        return true;
    }

    IterationKind kind() const { return static_cast<IterationKind>(internalField(Field::Kind).get().asUInt32AsAnyInt()); }
    JSObject* iteratedObject() const { return jsCast<JSObject*>(internalField(Field::IteratedObject).get()); }
    JSCell* storage() const { return internalField(Field::Storage).get().asCell(); }
    uint32_t entry() const { return static_cast<uint32_t>(internalField(Field::Entry).get().asNumber()); }

    void setIteratedObject(VM& vm, JSMap* map) { internalField(Field::IteratedObject).set(vm, this, map); }
    void setStorage(VM& vm, JSCell* butterfly) { internalField(Field::Storage).set(vm, this, butterfly); }
    void setEntry(VM& vm, uint32_t entry) { internalField(Field::Entry).set(vm, this, jsNumber(entry)); }

private:
    JSMapIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(JSGlobalObject*, JSMap*, IterationKind);
    void finishCreation(VM&);
    DECLARE_VISIT_CHILDREN;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSMapIterator);

} // namespace JSC
