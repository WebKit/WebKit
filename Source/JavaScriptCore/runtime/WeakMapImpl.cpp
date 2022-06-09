/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>.
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
#include "WeakMapImpl.h"

#include "AuxiliaryBarrierInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"
#include "WeakMapImplInlines.h"

namespace JSC {

template <typename WeakMapBucket>
void WeakMapImpl<WeakMapBucket>::destroy(JSCell* cell)
{
    static_cast<WeakMapImpl*>(cell)->~WeakMapImpl();
}

template<typename WeakMapBucket>
template<typename Visitor>
void WeakMapImpl<WeakMapBucket>::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    WeakMapImpl* thisObject = jsCast<WeakMapImpl*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.reportExtraMemoryVisited(thisObject->m_capacity * sizeof(WeakMapBucket));
}

DEFINE_VISIT_CHILDREN_WITH_MODIFIER(template<typename WeakMapBucket>, WeakMapImpl<WeakMapBucket>);

template <typename WeakMapBucket>
size_t WeakMapImpl<WeakMapBucket>::estimatedSize(JSCell* cell, VM& vm)
{
    auto* thisObject = static_cast<WeakMapImpl*>(cell);
    return Base::estimatedSize(thisObject, vm) + (sizeof(WeakMapImpl) - sizeof(Base)) + thisObject->m_capacity * sizeof(WeakMapBucket);
}

template <>
template <>
void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::visitOutputConstraints(JSCell*, AbstractSlotVisitor&)
{
    // Only JSWeakMap needs to harvest value references
}

template <>
template <>
void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::visitOutputConstraints(JSCell*, SlotVisitor&)
{
    // Only JSWeakMap needs to harvest value references
}

template<typename BucketType>
template<typename Visitor>
ALWAYS_INLINE void WeakMapImpl<BucketType>::visitOutputConstraints(JSCell* cell, Visitor& visitor)
{
    static_assert(std::is_same<BucketType, WeakMapBucket<WeakMapBucketDataKeyValue>>::value);

    auto* thisObject = jsCast<WeakMapImpl*>(cell);
    Locker locker { thisObject->cellLock() };
    auto* buffer = thisObject->buffer();
    for (uint32_t index = 0; index < thisObject->m_capacity; ++index) {
        auto* bucket = buffer + index;
        if (bucket->isEmpty() || bucket->isDeleted())
            continue;
        if (!visitor.isMarked(bucket->key()))
            continue;
        bucket->visitAggregate(visitor);
    }
}

template void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::visitOutputConstraints(JSCell*, AbstractSlotVisitor&);
template void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::visitOutputConstraints(JSCell*, SlotVisitor&);

template <typename WeakMapBucket>
template<typename Appender>
void WeakMapImpl<WeakMapBucket>::takeSnapshotInternal(unsigned limit, Appender appender)
{
    DisallowGC disallowGC;
    unsigned fetched = 0;
    forEach([&] (JSObject* key, JSValue value) {
        appender(key, value);
        ++fetched;
        if (limit && fetched >= limit)
            return IterationState::Stop;
        return IterationState::Continue;
    });
}

// takeSnapshot must not invoke garbage collection since iterating WeakMap may be modified.
template <>
void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::takeSnapshot(MarkedArgumentBuffer& buffer, unsigned limit)
{
    takeSnapshotInternal(limit, [&] (JSObject* key, JSValue) {
        buffer.append(key);
    });
}

template <>
void WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>::takeSnapshot(MarkedArgumentBuffer& buffer, unsigned limit)
{
    takeSnapshotInternal(limit, [&] (JSObject* key, JSValue value) {
        buffer.append(key);
        buffer.append(value);
    });
}

template class WeakMapImpl<WeakMapBucket<WeakMapBucketDataKeyValue>>;
template class WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>;

} // namespace JSC
