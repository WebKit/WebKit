/*
 * Copyright (C) 2013, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WeakMapBase.h"

#include "ExceptionHelpers.h"
#include "JSCInlines.h"

#include <wtf/MathExtras.h>

namespace JSC {

const ClassInfo WeakMapBase::s_info = { "WeakMapBase", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WeakMapBase) };

WeakMapBase::WeakMapBase(VM& vm, Structure* structure)
    : Base(vm, structure)
{
    ASSERT(m_deadKeyCleaner.target() == this);
}

void WeakMapBase::destroy(JSCell* cell)
{
    static_cast<WeakMapBase*>(cell)->~WeakMapBase();
}

size_t WeakMapBase::estimatedSize(JSCell* cell)
{
    auto* thisObj = jsCast<WeakMapBase*>(cell);
    return Base::estimatedSize(cell) + (thisObj->m_map.capacity() * (sizeof(JSObject*) + sizeof(WriteBarrier<Unknown>)));
}

void WeakMapBase::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);
    auto* thisObj = jsCast<WeakMapBase*>(cell);
    visitor.addUnconditionalFinalizer(&thisObj->m_deadKeyCleaner);
    visitor.addWeakReferenceHarvester(&thisObj->m_deadKeyCleaner);

    // Rough approximation of the external storage needed for the hashtable.
    // This isn't exact, but it is close enough, and proportional to the actual
    // external memory usage.
    visitor.reportExtraMemoryVisited(thisObj->m_map.capacity() * (sizeof(JSObject*) + sizeof(WriteBarrier<Unknown>)));
}

void WeakMapBase::set(VM& vm, JSObject* key, JSValue value)
{
    // Here we force the write barrier on the key.
    auto result = m_map.add(WriteBarrier<JSObject>(vm, this, key).get(), WriteBarrier<Unknown>());
    result.iterator->value.set(vm, this, value);
}

JSValue WeakMapBase::get(JSObject* key)
{
    auto iter = m_map.find(key);
    if (iter == m_map.end())
        return jsUndefined();
    return iter->value.get();
}

bool WeakMapBase::remove(JSObject* key)
{
    return m_map.remove(key);
}

bool WeakMapBase::contains(JSObject* key)
{
    return m_map.contains(key);
}

void WeakMapBase::clear()
{
    m_map.clear();
}

inline WeakMapBase* WeakMapBase::DeadKeyCleaner::target()
{
    return bitwise_cast<WeakMapBase*>(bitwise_cast<char*>(this) - OBJECT_OFFSETOF(WeakMapBase, m_deadKeyCleaner));
}

void WeakMapBase::DeadKeyCleaner::visitWeakReferences(SlotVisitor& visitor)
{
    WeakMapBase* map = target();
    m_liveKeyCount = 0;
    for (auto& pair : map->m_map) {
        if (!Heap::isMarked(pair.key))
            continue;
        m_liveKeyCount++;
        visitor.append(pair.value);
    }
    ASSERT(m_liveKeyCount <= map->m_map.size());
}

void WeakMapBase::DeadKeyCleaner::finalizeUnconditionally()
{
    WeakMapBase* map = target();
    if (m_liveKeyCount > map->m_map.size() / 2) {
        RELEASE_ASSERT(m_liveKeyCount <= map->m_map.size());
        int deadCount = map->m_map.size() - m_liveKeyCount;
        if (!deadCount)
            return;
        Vector<JSObject*> deadEntries;
        deadEntries.reserveCapacity(deadCount);
        for (auto& pair : map->m_map) {
            if (Heap::isMarked(pair.key))
                continue;
            deadEntries.uncheckedAppend(pair.key);
        }
        for (auto& deadEntry : deadEntries)
            map->m_map.remove(deadEntry);
    } else {
        MapType newMap;
        for (auto& pair : map->m_map) {
            if (!Heap::isMarked(pair.key))
                continue;
            newMap.add(pair.key, pair.value);
        }
        map->m_map.swap(newMap);
    }
}

}
