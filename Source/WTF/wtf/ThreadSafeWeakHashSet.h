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
    ThreadSafeWeakHashSet() = default;
    ThreadSafeWeakHashSet(ThreadSafeWeakHashSet&& other) { moveFrom(WTFMove(other)); }
    ThreadSafeWeakHashSet& operator=(ThreadSafeWeakHashSet&& other)
    {
        moveFrom(WTFMove(other));
        return *this;
    }

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        const_iterator(Vector<Ref<T>>&& strongReferences)
            : m_strongReferences(WTFMove(strongReferences)) { }

    public:
        T* get() const
        {
            RELEASE_ASSERT(m_position < m_strongReferences.size());
            return m_strongReferences[m_position].ptr();
        }
        T& operator*() const { return *get(); }
        T* operator->() const { return get(); }

        const_iterator& operator++()
        {
            RELEASE_ASSERT(m_position < m_strongReferences.size());
            ++m_position;
            return *this;
        }

        bool operator==(const const_iterator& other) const
        {
            // This should only be used to compare with end.
            ASSERT_UNUSED(other, other.m_strongReferences.isEmpty());
            return m_position == m_strongReferences.size();
        }

    private:
        template<typename> friend class ThreadSafeWeakHashSet;

        Vector<Ref<T>> m_strongReferences;
        size_t m_position { 0 };
    };

    const_iterator begin() const
    {
        return { values() };
    }

    const_iterator end() const { return { { } }; }

    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    typename HashSet<std::pair<RefPtr<ThreadSafeWeakPtrControlBlock>, uint16_t>>::AddResult add(const U& value)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!value.controlBlock().objectHasBeenDeleted());
        Locker locker { m_lock };
        auto& controlBlock = value.controlBlock();
        RefPtr retainedControlBlock { &controlBlock };
        amortizedCleanupIfNeeded();
        return m_set.add({ WTFMove(retainedControlBlock), controlBlock.objectOffset(static_cast<const T*>(&value)) });
    }

    template<typename U, std::enable_if_t<std::is_convertible_v<U*, T*>>* = nullptr>
    bool remove(const U& value)
    {
        Locker locker { m_lock };
        auto& controlBlock = value.controlBlock();
        RefPtr retainedControlBlock { &controlBlock };
        amortizedCleanupIfNeeded();
        return m_set.remove({ &value.controlBlock(), controlBlock.objectOffset(static_cast<const T*>(&value)) });
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
        auto& controlBlock = value.controlBlock();
        RefPtr retainedControlBlock { &controlBlock };
        amortizedCleanupIfNeeded();
        return m_set.contains({ WTFMove(retainedControlBlock), controlBlock.objectOffset(static_cast<const T*>(&value)) });
    }

    bool isEmptyIgnoringNullReferences() const
    {
        Locker locker { m_lock };
        amortizedCleanupIfNeeded();
        for (auto& pair : m_set) {
            auto& controlBlock = pair.first;
            if (!controlBlock->objectHasBeenDeleted())
                return false;
        }
        return true;
    }

    Vector<Ref<T>> values() const
    {
        Vector<Ref<T>> strongReferences;
        {
            Locker locker { m_lock };
            strongReferences.reserveInitialCapacity(m_set.size());
            m_set.removeIf([&] (auto& pair) {
                auto& controlBlock = pair.first;
                auto objectOffset = pair.second;
                if (auto refPtr = controlBlock->template makeStrongReferenceIfPossible<T>(objectOffset)) {
                    strongReferences.uncheckedAppend(refPtr.releaseNonNull());
                    return false;
                }
                return true;
            });
            m_operationCountSinceLastCleanup = 0;
        }
        return strongReferences;
    }

    template<typename Functor>
    void forEach(const Functor& callback) const
    {
        for (auto& item : values())
            callback(item.get());
    }

private:
    ALWAYS_INLINE void moveFrom(ThreadSafeWeakHashSet&& other)
    {
        Locker locker { m_lock };
        Locker otherLocker { other.m_lock };
        m_set = std::exchange(other.m_set, { });
        m_operationCountSinceLastCleanup = std::exchange(other.m_operationCountSinceLastCleanup, 0);
    }

    ALWAYS_INLINE void amortizedCleanupIfNeeded() const WTF_REQUIRES_LOCK(m_lock)
    {
        if (++m_operationCountSinceLastCleanup / 2 > m_set.size()) {
            m_set.removeIf([] (auto& pair) {
                return pair.first->objectHasBeenDeleted();
            });
            m_operationCountSinceLastCleanup = 0;
        }
    }

    // FIXME: Make PairHashTraits and RefHashTraits work together and change this back to a Ref<ThreadSafeWeakPtrControlBlock>.
    // We only add non-null pointers.
    mutable HashSet<std::pair<RefPtr<ThreadSafeWeakPtrControlBlock>, uint16_t>> m_set WTF_GUARDED_BY_LOCK(m_lock);
    mutable unsigned m_operationCountSinceLastCleanup WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    mutable Lock m_lock;
};

} // namespace WTF

using WTF::ThreadSafeWeakHashSet;
