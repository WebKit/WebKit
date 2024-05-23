/*
 * Copyright (C) 2013-2022 Apple, Inc. All rights reserved.
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
#include "JSSet.h"

namespace JSC {

const static uint8_t JSSetIteratorNumberOFInternalFields = 4;

class JSSetIterator final : public JSInternalFieldObjectImpl<JSSetIteratorNumberOFInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSSetIteratorNumberOFInternalFields>;

    DECLARE_EXPORT_INFO;

    enum class Field : uint8_t {
        Entry = 0,
        IteratedObject,
        Storage,
        Kind,
    };
    static_assert(numberOfInternalFields == JSSetIteratorNumberOFInternalFields);

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
        return vm.setIteratorSpace<mode>();
    }

    inline static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSSetIterator* create(JSGlobalObject* globalObject, Structure* structure, JSSet* iteratedObject, IterationKind kind)
    {
        VM& vm = getVM(globalObject);
        JSSetIterator* instance = new (NotNull, allocateCell<JSSetIterator>(vm)) JSSetIterator(vm, structure);
        instance->finishCreation(globalObject, iteratedObject, kind);
        return instance;
    }

    static JSSetIterator* createWithInitialValues(VM&, Structure*);

    JSValue nextWithAdvance(VM& vm, uint32_t entry)
    {
        JSCell* storageCell = storage();
        if (storageCell == vm.orderedHashTableSentinel())
            return { };

        JSSet::Handle table = jsCast<JSSet::Storage*>(storageCell);
        auto [newTable, nextKey, _, nextEntry] = table.getTableKeyValueEntry(vm, entry);
        if (nextKey.isEmpty()) {
            setStorage(vm, vm.orderedHashTableSentinel());
            return { };
        }

        setEntry(vm, nextEntry.asUInt32() + 1);
        if (newTable.storage() != table.storage())
            setStorage(vm, newTable.storage());
        return nextKey;
    }

    bool next(JSGlobalObject* globalObject, JSValue& value)
    {
        JSValue nextKey = nextWithAdvance(globalObject->vm(), entry());
        if (nextKey.isEmpty())
            return false;

        switch (kind()) {
        case IterationKind::Values:
        case IterationKind::Keys:
            value = nextKey;
            break;
        case IterationKind::Entries:
            value = createTuple(globalObject, nextKey, nextKey);
            break;
        }
        return true;
    }

    IterationKind kind() const { return static_cast<IterationKind>(internalField(Field::Kind).get().asUInt32AsAnyInt()); }
    JSObject* iteratedObject() const { return jsCast<JSObject*>(internalField(Field::IteratedObject).get()); }
    JSCell* storage() const { return internalField(Field::Storage).get().asCell(); }
    uint32_t entry() const { return static_cast<uint32_t>(internalField(Field::Entry).get().asNumber()); }

    void setIteratedObject(VM& vm, JSSet* set) { internalField(Field::IteratedObject).set(vm, this, set); }
    void setStorage(VM& vm, JSCell* butterfly) { internalField(Field::Storage).set(vm, this, butterfly); }
    void setEntry(VM& vm, uint32_t entry) { internalField(Field::Entry).set(vm, this, jsNumber(entry)); }

private:
    JSSetIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    JS_EXPORT_PRIVATE void finishCreation(JSGlobalObject*, JSSet*, IterationKind);
    void finishCreation(VM&);
    JS_EXPORT_PRIVATE JSValue createPair(JSGlobalObject*, JSValue, JSValue);
    DECLARE_VISIT_CHILDREN;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSSetIterator);

} // namespace JSC
