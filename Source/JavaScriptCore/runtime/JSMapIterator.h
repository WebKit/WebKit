/*
 * Copyright (C) 2013, 2016 Apple, Inc. All rights reserved.
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
#include "JSMap.h"
#include "JSObject.h"

namespace JSC {

// Now, it is only used for serialization.
class JSMapIterator final : public JSCell {
    typedef HashMapBucket<HashMapBucketDataKeyValue> HashMapBucketType;
public:
    using Base = JSCell;

    DECLARE_EXPORT_INFO;

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.mapIteratorSpace<mode>();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSMapIterator* create(VM& vm, Structure* structure, JSMap* iteratedObject, IterationKind kind)
    {
        JSMapIterator* instance = new (NotNull, allocateCell<JSMapIterator>(vm.heap)) JSMapIterator(vm, structure, iteratedObject, kind);
        instance->finishCreation(vm, iteratedObject);
        return instance;
    }

    ALWAYS_INLINE HashMapBucketType* advanceIter(JSGlobalObject* globalObject)
    {
        HashMapBucketType* prev = m_iter.get();
        if (!prev)
            return nullptr;
        VM& vm = getVM(globalObject);
        HashMapBucketType* bucket = m_iter->next();
        while (bucket && bucket->deleted())
            bucket = bucket->next();
        if (!bucket) {
            setIterator(vm, nullptr);
            return nullptr;
        }
        setIterator(vm, bucket); // We keep m_iter on the last value since the first thing we do in this function is call next().
        return bucket;
    }
    bool next(JSGlobalObject* globalObject, JSValue& value)
    {
        HashMapBucketType* bucket = advanceIter(globalObject);
        if (!bucket)
            return false;

        if (m_kind == IterationKind::Values)
            value = bucket->value();
        else if (m_kind == IterationKind::Keys)
            value = bucket->key();
        else
            value = createPair(globalObject, bucket->key(), bucket->value());
        return true;
    }

    bool nextKeyValue(JSGlobalObject* globalObject, JSValue& key, JSValue& value)
    {
        HashMapBucketType* bucket = advanceIter(globalObject);
        if (!bucket)
            return false;

        key = bucket->key();
        value = bucket->value();
        return true;
    }

    IterationKind kind() const { return m_kind; }
    JSValue iteratedValue() const { return m_map.get(); }

private:
    JSMapIterator(VM& vm, Structure* structure, JSMap*, IterationKind kind)
        : Base(vm, structure)
        , m_kind(kind)
    { }

    void setIterator(VM& vm, HashMapBucketType* bucket)
    {
        m_iter.setMayBeNull(vm, this, bucket);
    }

    JS_EXPORT_PRIVATE void finishCreation(VM&, JSMap*);
    JSValue createPair(JSGlobalObject*, JSValue, JSValue);
    static void visitChildren(JSCell*, SlotVisitor&);

    WriteBarrier<JSMap> m_map;
    WriteBarrier<HashMapBucketType> m_iter;
    IterationKind m_kind;
};
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSMapIterator);

} // namespace JSC
