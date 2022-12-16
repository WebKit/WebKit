/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <wtf/Algorithms.h>
#include <wtf/HashSet.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WTF {

template<typename T>
class ThreadSafeWeakHashSet final {
    WTF_MAKE_FAST_ALLOCATED;
public:

    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    typename HashSet<Ref<ThreadSafeWeakPtrControlBlock<T>>>::AddResult add(const U& value)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!value.m_controlBlock.objectHasBeenDeleted());
        Locker locker { m_lock };
        amortizedCleanupIfNeeded();
        return m_set.add(reinterpret_cast<ThreadSafeWeakPtrControlBlock<T>&>(value.m_controlBlock));
    }

    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    bool remove(const U& value)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!value.m_controlBlock.objectHasBeenDeleted());
        Locker locker { m_lock };
        amortizedCleanupIfNeeded();
        return m_set.remove(reinterpret_cast<ThreadSafeWeakPtrControlBlock<T>&>(value.m_controlBlock));
    }

    void clear()
    {
        Locker locker { m_lock };
        m_set.clear();
        m_operationCountSinceLastCleanup = 0;
    }

    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    bool contains(const U& value) const
    {
        Locker locker { m_lock };
        amortizedCleanupIfNeeded();
        return m_set.contains(value.m_controlBlock);
    }

    bool computesEmpty() const
    {
        Locker locker { m_lock };
        amortizedCleanupIfNeeded();
        for (auto& controlBlock : m_set) {
            if (!controlBlock->objectHasBeenDeleted())
                return false;
        }
        return true;
    }

    void forEach(const Function<void(T&)>& callback)
    {
        Vector<Ref<T>> strongReferences;
        {
            Locker locker { m_lock };
            strongReferences.reserveInitialCapacity(m_set.size());
            m_set.removeIf([&] (auto& controlBlock) {
                if (auto refPtr = controlBlock->makeStrongReferenceIfPossible()) {
                    strongReferences.uncheckedAppend(refPtr.releaseNonNull());
                    return false;
                }
                return true;
            });
            m_operationCountSinceLastCleanup = 0;
        }

        for (auto& item : strongReferences)
            callback(item.get());
    }

private:

    ALWAYS_INLINE void amortizedCleanupIfNeeded() const WTF_REQUIRES_LOCK(m_lock)
    {
        if (++m_operationCountSinceLastCleanup / 2 > m_set.size()) {
            m_set.removeIf([] (auto& value) {
                return value.get().objectHasBeenDeleted();
            });
            m_operationCountSinceLastCleanup = 0;
        }
    }

    mutable HashSet<Ref<ThreadSafeWeakPtrControlBlock<T>>> m_set WTF_GUARDED_BY_LOCK(m_lock);
    mutable unsigned m_operationCountSinceLastCleanup WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable Lock m_lock;
};

} // namespace WTF

using WTF::ThreadSafeWeakHashSet;
