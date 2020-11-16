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

#include <wtf/Algorithms.h>
#include <wtf/HashSet.h>
#include <wtf/WeakPtr.h>

namespace WTF {

template<typename Counter> struct HashTraits<Ref<WeakPtrImpl<Counter>>> : RefHashTraits<WeakPtrImpl<Counter>> {
    static constexpr bool hasIsReleasedWeakValueFunction = true;
    static bool isReleasedWeakValue(const Ref<WeakPtrImpl<Counter>>& value)
    {
        return !value.isHashTableDeletedValue() && !value.isHashTableEmptyValue() && !value.get();
    }
};

template<typename T, typename Counter = EmptyCounter>
class WeakHashSet final {
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef HashSet<Ref<WeakPtrImpl<Counter>>> WeakPtrImplSet;
    typedef typename WeakPtrImplSet::AddResult AddResult;

    class WeakHashSetConstIterator : public std::iterator<std::forward_iterator_tag, T, std::ptrdiff_t, const T*, const T&> {
    private:
        WeakHashSetConstIterator(const WeakPtrImplSet& set, typename WeakPtrImplSet::const_iterator position)
            : m_position(position), m_endPosition(set.end())
        {
            skipEmptyBuckets();
        }

    public:
        T* get() const { return static_cast<T*>((*m_position)->template get<T>()); }
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
            while (m_position != m_endPosition && !get())
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
        template <typename, typename> friend class WeakHashSet;

        typename WeakPtrImplSet::const_iterator m_position;
        typename WeakPtrImplSet::const_iterator m_endPosition;
    };
    typedef WeakHashSetConstIterator const_iterator;

    WeakHashSet() { }

    const_iterator begin() const { return WeakHashSetConstIterator(m_set, m_set.begin()); }
    const_iterator end() const { return WeakHashSetConstIterator(m_set, m_set.end()); }

    template <typename U>
    AddResult add(const U& value)
    {
        return m_set.add(*makeWeakPtr<T>(const_cast<U&>(value)).m_impl);
    }

    template <typename U>
    bool remove(const U& value)
    {
        auto& weakPtrImpl = value.weakPtrFactory().m_impl;
        if (!weakPtrImpl || !*weakPtrImpl)
            return false;
        return m_set.remove(*weakPtrImpl);
    }

    void clear() { m_set.clear(); }

    template <typename U>
    bool contains(const U& value) const
    {
        auto& weakPtrImpl = value.weakPtrFactory().m_impl;
        if (!weakPtrImpl || !*weakPtrImpl)
            return false;
        return m_set.contains(*weakPtrImpl);
    }

    unsigned capacity() const { return m_set.capacity(); }

    bool computesEmpty() const { return begin() == end(); }

    bool hasNullReferences() const
    {
        return WTF::anyOf(m_set, [] (auto& value) { return !value.get(); });
    }

    unsigned computeSize() const
    {
        const_cast<WeakPtrImplSet&>(m_set).removeIf([] (auto& value) { return !value.get(); });
        return m_set.size();
    }

    void forEach(const Function<void(T&)>& callback)
    {
        for (auto& item : map(m_set, [](auto& item) { return makeWeakPtr(item->template get<T>()); })) {
            if (item && m_set.contains(*item.m_impl))
                callback(*item);
        }
    }

#if ASSERT_ENABLED
    void checkConsistency() const { m_set.checkConsistency(); }
#else
    void checkConsistency() const { }
#endif

private:
    WeakPtrImplSet m_set;
};

template<typename MapFunction, typename T>
struct Mapper<MapFunction, const WeakHashSet<T> &, void> {
    using SourceItemType = WeakPtr<T>;
    using DestinationItemType = typename std::result_of<MapFunction(SourceItemType&)>::type;

    static Vector<DestinationItemType> map(const WeakHashSet<T>& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        result.reserveInitialCapacity(source.computeSize());
        for (auto& item : source)
            result.uncheckedAppend(mapFunction(item));
        return result;
    }
};

template<typename T>
inline auto copyToVector(const WeakHashSet<T>& collection) -> Vector<WeakPtr<T>>
{
    return WTF::map(collection, [] (auto& v) -> WeakPtr<T> { return makeWeakPtr<T>(v); });
}


} // namespace WTF

using WTF::WeakHashSet;
