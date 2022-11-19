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
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WTF {

template<typename T, typename WeakPtrImpl, typename = void> struct WeakHashSetBase {
    constexpr static bool isThreadSafe = false;
    using WeakPtrImplSet = HashSet<Ref<WeakPtrImpl>>;
    using WeakPtrType = WeakPtr<T, WeakPtrImpl>;
    using IteratorNextItemStorage = T*;
    template <typename V, EnableWeakPtrThreadingAssertions assertionsPolicy>
    static typename WeakPtrImplSet::AddResult add(WeakPtrImplSet& set, const V& value)
    {
        return set.add(*static_cast<const T&>(value).weakPtrFactory().template createWeakPtr<T>(const_cast<V&>(value), assertionsPolicy).m_impl);
    }
    using LockerType = std::unique_ptr<int>;
    using LockType = int*;
    int* lockIfThreadSafe() const { return nullptr; }
    template <typename V> static RefPtr<WeakPtrImpl> valueToImpl(V& value)
    {
        auto& weakPtrImpl = value.weakPtrFactory().m_impl;
        if (auto* pointer = weakPtrImpl.pointer(); pointer && *pointer)
            return pointer;
        return nullptr;
    }
    static WeakPtrType implToWeakPtr(const Ref<WeakPtrImpl>& impl) { return WeakPtrType { static_cast<T*>(impl->template get<T>()) }; }

    WeakPtrImplSet m_set;
    mutable unsigned m_operationCountSinceLastCleanup { 0 };
};

template<typename T, typename U> struct WeakHashSetBase<T, U, std::void_t<typename T::ThreadSafeWeakPtrCheck>> {
    constexpr static bool isThreadSafe = true;
    using WeakPtrImplSet = HashSet<Ref<ThreadSafeWeakPtrControlBlock<T>>>;
    using WeakPtrType = ThreadSafeWeakPtr<T>;
    using IteratorNextItemStorage = RefPtr<T>;
    template <typename V, EnableWeakPtrThreadingAssertions>
    static typename WeakPtrImplSet::AddResult add(WeakPtrImplSet& set, const V& value)
    {
        return set.add(value.m_controlBlock);
    }
    using LockerType = Locker<Lock>;
    using LockType = Lock;
    Lock& lockIfThreadSafe() const WTF_RETURNS_LOCK(m_lock) { return m_lock; }
    template <typename V> static RefPtr<ThreadSafeWeakPtrControlBlock<T>> valueToImpl(V& value)
    {
        return &value.m_controlBlock;
    }
    static WeakPtrType implToWeakPtr(const Ref<ThreadSafeWeakPtrControlBlock<T>>& impl) { return WeakPtrType { impl->template get<T>() }; }

    mutable Lock m_lock;
    mutable unsigned m_operationCountSinceLastCleanup WTF_GUARDED_BY_LOCK(m_lock) { 0 };
    WeakPtrImplSet m_set WTF_GUARDED_BY_LOCK(m_lock);
};

template<typename T, typename WeakPtrImpl = DefaultWeakPtrImpl, EnableWeakPtrThreadingAssertions assertionsPolicy = EnableWeakPtrThreadingAssertions::Yes>
class WeakHashSet final : public WeakHashSetBase<T, WeakPtrImpl> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Base = WeakHashSetBase<T, WeakPtrImpl>;
    using LockerType = typename Base::LockerType;
    
    WeakHashSet() { }

    // FIXME: Do the old iteration style if it's not thread safe.
    class WeakHashSetConstIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        WeakHashSetConstIterator(bool end, Vector<typename Base::WeakPtrType>&& snapshot)
            : m_end(end)
            , m_snapshot(WTFMove(snapshot))
        {
            skipEmptyBuckets();
        }

    public:
        T* get() const
        {
            ASSERT(m_nextItem);
            if constexpr (Base::isThreadSafe)
                return m_nextItem.get();
            else
                return m_nextItem;
        }
        T& operator*() const { return *get(); }
        T* operator->() const { return get(); }

        WeakHashSetConstIterator& operator++()
        {
            ASSERT(m_position < m_snapshot.size());
            ASSERT(m_nextItem);
            m_nextItem = nullptr;
            ++m_position;
            skipEmptyBuckets();
            return *this;
        }

        void skipEmptyBuckets()
        {
            while (m_position < m_snapshot.size()) {
                if (auto item = m_snapshot[m_position].get()) {
                    m_nextItem = item;
                    return;
                }
                ++m_position;
            }
        }

        bool operator==(const WeakHashSetConstIterator& other) const
        {
            ASSERT(other.m_end);
            return !m_nextItem;
        }

        bool operator!=(const WeakHashSetConstIterator& other) const
        {
            return !(*this == other);
        }

    private:
        template <typename, typename, EnableWeakPtrThreadingAssertions> friend class WeakHashSet;

        bool m_end;
        Vector<typename Base::WeakPtrType> m_snapshot;
        size_t m_position { 0 };
        typename Base::IteratorNextItemStorage m_nextItem;
    };
    using const_iterator = WeakHashSetConstIterator;

    const_iterator begin() const
    {
        return WeakHashSetConstIterator(false, weakPtrs());
    }

    const_iterator end() const
    {
        return WeakHashSetConstIterator(true, { });
    }

    template <typename U>
    typename Base::WeakPtrImplSet::AddResult add(const U& value)
    {
        amortizedCleanupIfNeeded();
        LockerType locker { Base::lockIfThreadSafe() };
        return Base::template add<U, assertionsPolicy>(set(), value);
    }

    template <typename U>
    bool remove(const U& value)
    {
        amortizedCleanupIfNeeded();
        if (auto impl = Base::valueToImpl(value)) {
            LockerType locker { Base::lockIfThreadSafe() };
            return set().remove(*impl);
        }
        return false;
    }

    void clear()
    {
        LockerType locker { Base::lockIfThreadSafe() };
        set().clear();
        Base::m_operationCountSinceLastCleanup = 0;
    }

    template <typename U>
    bool contains(const U& value) const
    {
        LockerType locker { Base::lockIfThreadSafe() };
        ++Base::m_operationCountSinceLastCleanup;
        if (auto impl = Base::valueToImpl(value))
            return set().contains(*impl);
        return false;
    }

    unsigned capacity() const
    {
        LockerType locker { Base::lockIfThreadSafe() };
        return set().capacity();
    }

    bool computesEmpty() const
    {
        return !computeSize();
    }

    bool hasNullReferences() const
    {
        LockerType locker { Base::lockIfThreadSafe() };
        return WTF::anyOf(set(), [] (auto& value) { return !value.get(); });
    }

    unsigned computeSize() const
    {
        const_cast<WeakHashSet&>(*this).removeNullReferences();
        LockerType locker { Base::lockIfThreadSafe() };
        return set().size();
    }

    void forEach(const Function<void(T&)>& callback)
    {
        return forEach(callback, weakPtrs());
    }

#if ASSERT_ENABLED
    void checkConsistency() const
    {
        LockerType locker { Base::lockIfThreadSafe() };
        set().checkConsistency();
    }
#else
    void checkConsistency() const { }
#endif

private:
    template<typename, typename, typename> friend struct Mapper;

    decltype(Base::m_set)& set() { return Base::m_set; }
    const decltype(Base::m_set)& set() const { return Base::m_set; }

    Vector<typename Base::WeakPtrType> weakPtrs() const
    {
        LockerType locker { Base::lockIfThreadSafe() };
        ++Base::m_operationCountSinceLastCleanup;
        return map(set(), Base::implToWeakPtr);
    }

    void forEach(const Function<void(T&)>& callback, const Vector<typename Base::WeakPtrType>& weakPtrs)
    {
        for (auto& weakPtr : weakPtrs) {
            if (auto item = weakPtr.get()) {

                // FIXME: Do we need this check? Are there really users that mutate the WeakHashSet
                // during iteration and expect those changes to be reflected during iteration?
                if constexpr (Base::isThreadSafe) {
                    if (!set().contains(*weakPtr.m_impl)) {
                        ASSERT_NOT_REACHED_WITH_MESSAGE("Don't mutate the WeakHashSet during iteration");
                        continue;
                    }
                }

                callback(*item);
            }
        }
    }

    ALWAYS_INLINE void removeNullReferences()
    {
        LockerType locker { Base::lockIfThreadSafe() };
        set().removeIf([] (auto& value) { return !value->template get<T>(); });
        Base::m_operationCountSinceLastCleanup = 0;
    }

    ALWAYS_INLINE void amortizedCleanupIfNeeded() const
    {
        unsigned currentCount;
        {
            LockerType locker { Base::lockIfThreadSafe() };
            currentCount = ++Base::m_operationCountSinceLastCleanup;
        }
        if (currentCount / 2 > set().size())
            const_cast<WeakHashSet&>(*this).removeNullReferences();
    }
};

template<typename MapFunction, typename T, typename WeakMapImpl>
struct Mapper<MapFunction, const WeakHashSet<T, WeakMapImpl> &, void> {
    using SourceItemType = T&;
    using DestinationItemType = typename std::invoke_result<MapFunction, SourceItemType&>::type;

    static Vector<DestinationItemType> map(const WeakHashSet<T, WeakMapImpl>& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        // FIXME: No need to allocate a vector here if it's not thread safe.
        auto weakPtrs = source.weakPtrs();
        result.reserveInitialCapacity(weakPtrs.size());
        for (auto& weakPtr : weakPtrs) {
            if (auto item = weakPtr.get())
                result.uncheckedAppend(mapFunction(*item));
        }
        return result;
    }
};

template<typename T, typename WeakMapImpl>
inline auto copyToVector(const WeakHashSet<T, WeakMapImpl>& collection) -> Vector<WeakPtr<T, WeakMapImpl>>
{
    return WTF::map(collection, [](auto& v) -> WeakPtr<T, WeakMapImpl> { return WeakPtr<T, WeakMapImpl> { v }; });
}


} // namespace WTF

using WTF::WeakHashSet;
