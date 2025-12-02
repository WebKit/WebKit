/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include <algorithm>
#include <wtf/CheckedPtr.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace WTF {

template<typename T, typename WeakPtrImplType>
class WeakHashSet final {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WeakHashSet);
public:
    using KeyType = WeakPtr<T, WeakPtrImplType>;
    using SetType = HashSet<KeyType>;
    using AddResult = typename SetType::AddResult;

    class WeakHashSetConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        WeakHashSetConstIterator(const WeakHashSet& set, typename SetType::const_iterator position)
            : m_set(&set)
            , m_position(position)
            , m_endPosition(set.m_set.end())
        {
            skipEmptyBuckets();
        }

    public:
        WeakHashSetConstIterator() = default;

        T* get() const { return m_position->get(); }
        T& operator*() const { return *get(); }
        T* operator->() const { return get(); }

        WeakHashSetConstIterator& operator++()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            skipEmptyBuckets();
            m_set->increaseOperationCountSinceLastCleanup();
            return *this;
        }

        WeakHashSetConstIterator operator++(int)
        {
            WeakHashSetConstIterator temp = *this;
            ++(*this);
            return temp;
        }

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && !get())
                ++m_position;
        }

        bool operator==(const WeakHashSetConstIterator& other) const
        {
            return m_position == other.m_position;
        }

    private:
        template <typename, typename> friend class WeakHashSet;

        const WeakHashSet* m_set { nullptr };
        typename SetType::const_iterator m_position;
        typename SetType::const_iterator m_endPosition;
    };
    typedef WeakHashSetConstIterator const_iterator;

    WeakHashSet() = default;

    const_iterator begin() const { return WeakHashSetConstIterator(*this, m_set.begin()); }
    const_iterator end() const { return WeakHashSetConstIterator(*this, m_set.end()); }

    const_iterator find(const T* value) const
    {
        increaseOperationCountSinceLastCleanup();
        return WeakHashSetConstIterator(*this, m_set.find(value));
    }

    const_iterator find(const T& value) const { return find(&value); }

    AddResult add(const T* value)
    {
        amortizedCleanupIfNeeded();
        return m_set.add(value);
    }

    AddResult add(const T& value) { return add(&value); }

    AddResult add(WeakRef<T>&& value)
    {
        amortizedCleanupIfNeeded();
        return m_set.add(WTFMove(value));
    }

    T* takeAny()
    {
        auto iterator = begin();
        if (iterator == end())
            return nullptr;
        return m_set.take(iterator.m_position).get();
    }

    bool remove(const T* value)
    {
        amortizedCleanupIfNeeded();
        return m_set.remove(value);
    }

    bool remove(const T& value) { return remove(&value); }

    bool remove(const_iterator iterator)
    {
        bool removed = m_set.remove(iterator.m_position);
        amortizedCleanupIfNeeded();
        return removed;
    }

    void clear()
    {
        m_set.clear();
        cleanupHappened();
    }

    bool contains(const T* value) const
    {
        increaseOperationCountSinceLastCleanup();
        return m_set.contains(value);
    }

    bool contains(const T& value) const { return contains(&value); }

    unsigned capacity() const { return m_set.capacity(); }

    bool isEmptyIgnoringNullReferences() const
    {
        if (m_set.isEmpty())
            return true;
        return begin() == end();
    }

    bool hasNullReferences() const
    {
        unsigned count = 0;
        auto result = std::ranges::any_of(m_set, [&](auto& value) {
            ++count;
            return !value;
        });
        if (result)
            increaseOperationCountSinceLastCleanup(count);
        else
            cleanupHappened();
        return result;
    }

    unsigned computeSize() const
    {
        const_cast<WeakHashSet&>(*this).removeNullReferences();
        return m_set.size();
    }

    void forEach(NOESCAPE const Function<void(T&)>& callback) requires HasRefPtrMemberFunctions<T>::value
    {
        auto items = compactMap(m_set, [](const KeyType& item) -> RefPtr<T> {
            return RefPtr { item.get() };
        });
        for (auto& item : items)
            callback(item.get());
    }

    void forEach(NOESCAPE const Function<void(T&)>& callback) requires (HasCheckedPtrMemberFunctions<T>::value && !HasRefPtrMemberFunctions<T>::value)
    {
        auto items = compactMap(m_set, [](const KeyType& item) -> CheckedPtr<T> {
            return CheckedPtr { item.get() };
        });
        for (auto& item : items)
            callback(item.get());
    }

#if ASSERT_ENABLED
    void checkConsistency() const { m_set.checkConsistency(); }
#else
    void checkConsistency() const { }
#endif

private:
    ALWAYS_INLINE void cleanupHappened() const
    {
        m_operationCountSinceLastCleanup = 0;
        m_maxOperationCountWithoutCleanup = std::min(std::numeric_limits<unsigned>::max() / 2, m_set.size()) * 2;
    }

    ALWAYS_INLINE bool removeNullReferences()
    {
        bool didRemove = m_set.removeIf([] (auto& value) { return !value.get(); });
        cleanupHappened();
        return didRemove;
    }

    ALWAYS_INLINE unsigned increaseOperationCountSinceLastCleanup(unsigned count = 1) const
    {
        unsigned currentCount = m_operationCountSinceLastCleanup += count;
        return currentCount;
    }

    ALWAYS_INLINE void amortizedCleanupIfNeeded() const
    {
        unsigned currentCount = increaseOperationCountSinceLastCleanup();
        if (currentCount > m_maxOperationCountWithoutCleanup)
            const_cast<WeakHashSet&>(*this).removeNullReferences();
    }

    SetType m_set;
    mutable unsigned m_operationCountSinceLastCleanup { 0 };
    mutable unsigned m_maxOperationCountWithoutCleanup { 0 };
};

template<typename T, typename U>
size_t containerSize(const WeakHashSet<T, U>& container) { return container.computeSize(); }

template<typename T, typename U>
inline auto copyToVector(const WeakHashSet<T, U>& collection) -> Vector<typename WeakHashSet<T, U>::KeyType>
{
    return map(collection, [](auto& v) -> typename WeakHashSet<T, U>::KeyType {
        return typename WeakHashSet<T, U>::KeyType { v };
    });
}

} // namespace WTF

using WTF::WeakHashSet;
