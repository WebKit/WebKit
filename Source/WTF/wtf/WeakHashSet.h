/*
 * Copyright (C) 2017, 2019 Apple Inc. All rights reserved.
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
#include <wtf/HashTraits.h>
#include <wtf/WeakPtr.h>

namespace WTF {

template <typename T>
class WeakHashSet {
public:
    typedef HashSet<Ref<WeakReference<T>>> WeakReferenceSet;

    class WeakHashSetConstIterator : public std::iterator<std::forward_iterator_tag, T, std::ptrdiff_t, const T*, const T&> {
    private:
        WeakHashSetConstIterator(const WeakReferenceSet& set, typename WeakReferenceSet::const_iterator position)
            : m_position(position), m_endPosition(set.end())
        {
            skipEmptyBuckets();
        }

    public:
        T* get() const { return m_position->get().get(); }
        T& operator*() const { return *get(); }
        T* operator->() const { return get(); }

        WeakHashSetConstIterator& operator++()
        {
            ASSERT(m_position != m_endPosition);
            ++m_position;
            skipEmptyBuckets();
            return *this;
        }

        void skipEmptyBuckets()
        {
            while (m_position != m_endPosition && !m_position->get().get())
                ++m_position;
        }

        bool operator==(const WeakHashSetConstIterator& other) const
        {
            return m_position == other.m_position;
        }

        bool operator!=(const WeakHashSetConstIterator& other) const
        {
            return m_position != other.m_position;
        }

    private:
        template <typename> friend class WeakHashSet;

        typename WeakReferenceSet::const_iterator m_position;
        typename WeakReferenceSet::const_iterator m_endPosition;
    };
    typedef WeakHashSetConstIterator const_iterator;

    WeakHashSet() { }

    const_iterator begin() const { return WeakHashSetConstIterator(m_set, m_set.begin()); }
    const_iterator end() const { return WeakHashSetConstIterator(m_set, m_set.end()); }

    template <typename U>
    void add(const U& value)
    {
        m_set.add(*makeWeakPtr<T>(const_cast<U&>(value)).m_ref);
    }

    template <typename U>
    bool remove(const U& value)
    {
        auto* weakReference = weak_reference_downcast<T>(value.weakPtrFactory().m_ref.get());
        if (!weakReference)
            return false;
        return m_set.remove(weakReference);
    }

    template <typename U>
    bool contains(const U& value) const
    {
        auto* weakReference = weak_reference_downcast<T>(value.weakPtrFactory().m_ref.get());
        if (!weakReference)
            return false;
        return m_set.contains(weakReference);
    }

    unsigned capacity() const { return m_set.capacity(); }

    bool computesEmpty() const
    {
        if (m_set.isEmpty())
            return true;
        for (auto& value : m_set) {
            if (value->get())
                return false;
        }
        return true;
    }

    bool hasNullReferences() const
    {
        return WTF::anyOf(m_set, [] (auto& value) { return !value->get(); });
    }

    unsigned computeSize() const
    {
        const_cast<WeakReferenceSet&>(m_set).removeIf([] (auto& value) { return !value->get(); });
        return m_set.size();
    }

#if ASSERT_DISABLED
    void checkConsistency() const { }
#else
    void checkConsistency() const { m_set.checkConsistency(); }
#endif

private:
    WeakReferenceSet m_set;
};

template<typename T> struct HashTraits<Ref<WeakReference<T>>> : RefHashTraits<WeakReference<T>> {
    static const bool hasIsReleasedWeakValueFunction = true;
    static bool isReleasedWeakValue(const Ref<WeakReference<T>>& value)
    {
        return !value.isHashTableDeletedValue() && !value.isHashTableEmptyValue() && !value.get().get();
    }
};

} // namespace WTF

using WTF::WeakHashSet;
