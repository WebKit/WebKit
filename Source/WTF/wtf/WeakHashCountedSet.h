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

#include <wtf/WeakHashMap.h>

namespace WTF {

template<typename Value, typename WeakPtrImpl = DefaultWeakPtrImpl>
class WeakHashCountedSet {
    WTF_MAKE_FAST_ALLOCATED;
private:
    using ImplType = WeakHashMap<Value, unsigned, WeakPtrImpl>;
public:
    using ValueType = Value;
    using iterator = typename ImplType::iterator;
    using const_iterator = typename ImplType::const_iterator;
    using AddResult = typename ImplType::AddResult;

    // Iterators iterate over pairs of values and counts.
    iterator begin() { return m_impl.begin(); }
    iterator end() { return m_impl.end(); }
    const_iterator begin() const { return m_impl.begin(); }
    const_iterator end() const { return m_impl.end(); }

    iterator find(const ValueType& value) { return m_impl.find(value); }
    const_iterator find(const ValueType& value) const { return m_impl.find(value); }
    bool contains(const ValueType& value) const { return m_impl.contains(value); }

    bool isEmptyIgnoringNullReferences() const { return m_impl.isEmptyIgnoringNullReferences(); }
    unsigned computeSize() const { return m_impl.computeSize(); }

    // Increments the count if an equal value is already present.
    // The return value includes both an iterator to the value's location,
    // and an isNewEntry bool that indicates whether it is a new or existing entry.
    AddResult add(const ValueType&);
    AddResult add(ValueType&&);

    // Decrements the count of the value, and removes it if count goes down to zero.
    // Returns true if the value is removed.
    bool remove(const ValueType&);
    bool remove(iterator);

    // Clears the whole set.
    void clear() { m_impl.clear(); }

private:
    WeakHashMap<Value, unsigned, WeakPtrImpl> m_impl;
};

template<typename Value, typename WeakPtrImpl>
inline auto WeakHashCountedSet<Value, WeakPtrImpl>::add(const ValueType &value) -> AddResult
{
    auto result = m_impl.add(value, 0);
    ++result.iterator->value;
    return result;
}

template<typename Value, typename WeakPtrImpl>
inline auto WeakHashCountedSet<Value, WeakPtrImpl>::add(ValueType&& value) -> AddResult
{
    auto result = m_impl.add(std::forward<Value>(value), 0);
    ++result.iterator->value;
    return result;
}

template<typename Value, typename WeakPtrImpl>
inline bool WeakHashCountedSet<Value, WeakPtrImpl>::remove(const ValueType& value)
{
    return remove(find(value));
}

template<typename Value, typename WeakPtrImpl>
inline bool WeakHashCountedSet<Value, WeakPtrImpl>::remove(iterator it)
{
    if (it == end())
        return false;

    unsigned oldVal = it->value;
    ASSERT(oldVal);
    unsigned newVal = oldVal - 1;
    if (newVal) {
        it->value = newVal;
        return false;
    }

    m_impl.remove(it);
    return true;
}

template<typename Value, typename WeakMapImpl>
size_t containerSize(const WeakHashCountedSet<Value, WeakMapImpl>& container) { return container.computeSize(); }

template<typename Value>
using SingleThreadWeakHashCountedSet = WeakHashCountedSet<Value, SingleThreadWeakPtrImpl>;

} // namespace WTF

using WTF::SingleThreadWeakHashCountedSet;
using WTF::WeakHashCountedSet;
