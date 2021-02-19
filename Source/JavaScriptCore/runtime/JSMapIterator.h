/*
 * Copyright (C) 2013-2021 Apple, Inc. All rights reserved.
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

class JSMapIterator final : public JSInternalFieldObjectImpl<3> {
public:
    using HashMapBucketType = HashMapBucket<HashMapBucketDataKeyValue>;
    using Base = JSInternalFieldObjectImpl<3>;

    DECLARE_EXPORT_INFO;

    enum class Field : uint8_t {
        MapBucket = 0,
        IteratedObject,
        Kind,
    };
    static_assert(numberOfInternalFields == 3);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsNull(),
            jsNull(),
            jsNumber(0),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.mapIteratorSpace<mode>();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSMapIteratorType, StructureFlags), info());
    }

    static JSMapIterator* create(VM& vm, Structure* structure, JSMap* iteratedObject, IterationKind kind)
    {
        JSMapIterator* instance = new (NotNull, allocateCell<JSMapIterator>(vm.heap)) JSMapIterator(vm, structure);
        instance->finishCreation(vm, iteratedObject, kind);
        return instance;
    }

    static JSMapIterator* createWithInitialValues(VM&, Structure*);

    ALWAYS_INLINE HashMapBucketType* advanceIter(VM& vm)
    {
        HashMapBucketType* prev = iterator();
        HashMapBucketType* sentinel = jsCast<HashMapBucketType*>(vm.sentinelMapBucket());
        if (prev == sentinel)
            return nullptr;
        HashMapBucketType* bucket = prev->next();
        while (bucket && bucket->deleted())
            bucket = bucket->next();
        if (!bucket) {
            setIterator(vm, sentinel);
            return nullptr;
        }
        setIterator(vm, bucket); // We keep iterator on the last value since the first thing we do in this function is call next().
        return bucket;
    }

    bool next(JSGlobalObject* globalObject, JSValue& value)
    {
        HashMapBucketType* bucket = advanceIter(getVM(globalObject));
        if (!bucket)
            return false;

        switch (kind()) {
        case IterationKind::Values:
            value = bucket->value();
            break;
        case IterationKind::Keys:
            value = bucket->key();
            break;
        case IterationKind::Entries:
            value = createPair(globalObject, bucket->key(), bucket->value());
            break;
        }
        return true;
    }

    bool nextKeyValue(JSGlobalObject* globalObject, JSValue& key, JSValue& value)
    {
        HashMapBucketType* bucket = advanceIter(getVM(globalObject));
        if (!bucket)
            return false;

        key = bucket->key();
        value = bucket->value();
        return true;
    }

    IterationKind kind() const { return static_cast<IterationKind>(internalField(Field::Kind).get().asUInt32AsAnyInt()); }
    JSObject* iteratedObject() const { return jsCast<JSObject*>(internalField(Field::IteratedObject).get()); }
    HashMapBucketType* iterator() const { return jsCast<HashMapBucketType*>(internalField(Field::MapBucket).get()); }

private:
    JSMapIterator(VM& vm, Structure* structure)
        : Base(vm, structure)
    { }

    void setIterator(VM& vm, HashMapBucketType* bucket)
    {
        internalField(Field::MapBucket).set(vm, this, bucket);
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSMap*, IterationKind);
    void finishCreation(VM&);
    JSValue createPair(JSGlobalObject*, JSValue, JSValue);
    DECLARE_VISIT_CHILDREN;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSMapIterator);

} // namespace JSC
