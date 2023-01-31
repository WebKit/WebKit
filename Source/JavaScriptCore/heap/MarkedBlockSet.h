/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#include "MarkedBlock.h"
#include "TinyBloomFilter.h"
#include <wtf/HashSet.h>

namespace JSC {

class MarkedBlock;

class MarkedBlockSet {
public:
    void add(MarkedBlock*);
    void remove(MarkedBlock*);

    unsigned size();
    bool contains(MarkedBlock*);

    template<typename Functor> bool forEachLiveCell(const Functor&);
    template<typename Functor> bool forEachDeadCell(const Functor&);

    template<typename Bits> bool filterRuleOut(Bits);

private:
    void recomputeFilter();

    TinyBloomFilter<uintptr_t> m_filter;
    HashSet<MarkedBlock*> m_set;
#if USE(JSVALUE32_64)
    Lock m_lock;
#endif
};

inline void MarkedBlockSet::add(MarkedBlock* block)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    m_filter.add(reinterpret_cast<uintptr_t>(block));
    m_set.add(block);
}

inline void MarkedBlockSet::remove(MarkedBlock* block)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    unsigned oldCapacity = m_set.capacity();
    m_set.remove(block);
    if (m_set.capacity() != oldCapacity) // Indicates we've removed a lot of blocks.
        recomputeFilter();
}

inline unsigned MarkedBlockSet::size()
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    return m_set.size();
}

inline bool MarkedBlockSet::contains(MarkedBlock* block)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    return m_set.contains(block);
}

inline void MarkedBlockSet::recomputeFilter()
{
    TinyBloomFilter<uintptr_t> filter;
    for (HashSet<MarkedBlock*>::iterator it = m_set.begin(); it != m_set.end(); ++it)
        filter.add(reinterpret_cast<uintptr_t>(*it));
    m_filter = filter;
}

template<typename Functor> inline bool MarkedBlockSet::forEachLiveCell(const Functor& functor)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    auto end = m_set.end();
    for (auto it = m_set.begin(); it != end; ++it) {
        IterationStatus result = (*it)->handle().forEachLiveCell(
            [&](size_t, HeapCell* cell, HeapCell::Kind kind) -> IterationStatus {
                return functor(cell, kind);
            });
        if (result == IterationStatus::Done)
            return true;
    }
    return false;
}

template<typename Functor> inline bool MarkedBlockSet::forEachDeadCell(const Functor& functor)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    auto end = m_set.end();
    for (auto it = m_set.begin(); it != end; ++it) {
        if ((*it)->handle().forEachDeadCell(functor) == IterationStatus::Done)
            return true;
    }
    return false;
}

template<typename Bits> bool MarkedBlockSet::filterRuleOut(Bits bits)
{
#if USE(JSVALUE32_64)
    Locker locker { m_lock };
#endif
    return m_filter.ruleOut(bits);
}

} // namespace JSC
