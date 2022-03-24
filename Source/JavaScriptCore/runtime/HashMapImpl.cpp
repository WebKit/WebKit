/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HashMapImpl.h"

#include "HashMapImplInlines.h"
#include "JSCInlines.h"

namespace JSC {

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(template<>, HashMapBucket<HashMapBucketDataKey>);
DEFINE_VISIT_CHILDREN_WITH_MODIFIER(template<>, HashMapBucket<HashMapBucketDataKeyValue>);

template<>
const ClassInfo HashMapBucket<HashMapBucketDataKey>::s_info =
    { "HashMapBucket"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(HashMapBucket<HashMapBucketDataKey>) };

template<>
const ClassInfo HashMapBucket<HashMapBucketDataKeyValue>::s_info =
    { "HashMapBucket"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(HashMapBucket<HashMapBucketDataKeyValue>) };

template<typename Data>
template<typename Visitor>
void HashMapBucket<Data>::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    HashMapBucket* thisObject = jsCast<HashMapBucket*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_next);
    visitor.append(thisObject->m_prev);

    static_assert(sizeof(Data) % sizeof(WriteBarrier<Unknown>) == 0, "We assume that these are filled with WriteBarrier<Unknown> members only.");
    visitor.appendValues(bitwise_cast<WriteBarrier<Unknown>*>(&thisObject->m_data), sizeof(Data) / sizeof(WriteBarrier<Unknown>));
}

template<typename HashMapBucket>
template<typename Visitor>
void HashMapImpl<HashMapBucket>::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    HashMapImpl* thisObject = jsCast<HashMapImpl*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_head);
    visitor.append(thisObject->m_tail);
    
    if (HashMapBufferType* buffer = thisObject->m_buffer.get())
        visitor.markAuxiliary(buffer);
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(template<typename HashMapBucket>, HashMapImpl<HashMapBucket>);

template <typename HashMapBucket>
size_t HashMapImpl<HashMapBucket>::estimatedSize(JSCell* cell, VM& vm)
{
    return Base::estimatedSize(cell, vm) + static_cast<HashMapImpl<HashMapBucket>*>(cell)->approximateSize();
}

const ClassInfo* getHashMapBucketKeyClassInfo()
{
    return &HashMapBucket<HashMapBucketDataKey>::s_info;
}
const ClassInfo* getHashMapBucketKeyValueClassInfo()
{
    return &HashMapBucket<HashMapBucketDataKeyValue>::s_info;
}

template class HashMapImpl<HashMapBucket<HashMapBucketDataKeyValue>>;
template class HashMapImpl<HashMapBucket<HashMapBucketDataKey>>;

} // namespace JSC
