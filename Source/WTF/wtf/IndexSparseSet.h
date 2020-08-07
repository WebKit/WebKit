/*
 * Copyright (C) 2015-2017 Apple Inc. All Rights Reserved.
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

#pragma once

#include <wtf/Vector.h>

namespace WTF {

template<typename KeyTypeArg, typename ValueTypeArg>
struct KeyValuePair;

// IndexSparseSet is an efficient set of integers that can only be valued
// between zero and size() - 1.
//
// The implementation is using Briggs Sparse Set representation. We allocate
// memory from 0 to size() - 1 to do mapping in O(1), but we never initialize
// that memory. When adding/removing values to the set, they are added in a list
// and the corresponding bucket is initialized to the position in the list.
//
// The assumption here is that we only need a sparse subset of number live at any
// time.

template<typename T>
struct DefaultIndexSparseSetTraits {
    typedef T EntryType;
    
    static T create(unsigned entry)
    {
        return entry;
    }
    
    static unsigned key(const T& entry)
    {
        return entry;
    }
};

template<typename KeyType, typename ValueType>
struct DefaultIndexSparseSetTraits<KeyValuePair<KeyType, ValueType>> {
    typedef KeyValuePair<KeyType, ValueType> EntryType;

    template<typename PassedValueType>
    static EntryType create(unsigned key, PassedValueType&& value)
    {
        return EntryType(key, std::forward<PassedValueType>(value));
    }
    
    static unsigned key(const EntryType& entry)
    {
        return entry.key;
    }
};

template<typename EntryType = unsigned, typename EntryTypeTraits = DefaultIndexSparseSetTraits<EntryType>, typename OverflowHandler = CrashOnOverflow>
class IndexSparseSet {
    WTF_MAKE_FAST_ALLOCATED;
    typedef Vector<EntryType, 0, OverflowHandler> ValueList;
public:
    explicit IndexSparseSet(unsigned size);

    template<typename... Arguments>
    bool add(unsigned, Arguments&&...);
    template<typename... Arguments>
    bool set(unsigned, Arguments&&...);
    bool remove(unsigned);
    void clear();

    unsigned size() const;
    bool isEmpty() const;
    bool contains(unsigned) const;
    const EntryType* get(unsigned) const;
    EntryType* get(unsigned);

    typedef typename ValueList::const_iterator const_iterator;
    const_iterator begin() const;
    const_iterator end() const;
    
    void sort();

    void validate();
    
    const ValueList& values() const { return m_values; }

private:
    Vector<unsigned, 0, OverflowHandler, 1> m_map;
    ValueList m_values;
};

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
inline IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::IndexSparseSet(unsigned size)
{
    m_map.grow(size);
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
template<typename... Arguments>
inline bool IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::add(unsigned value, Arguments&&... arguments)
{
    if (contains(value))
        return false;

    unsigned newPosition = m_values.size();
    m_values.append(EntryTypeTraits::create(value, std::forward<Arguments>(arguments)...));
    m_map[value] = newPosition;
    return true;
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
template<typename... Arguments>
inline bool IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::set(unsigned value, Arguments&&... arguments)
{
    if (EntryType* entry = get(value)) {
        *entry = EntryTypeTraits::create(value, std::forward<Arguments>(arguments)...);
        return false;
    }

    unsigned newPosition = m_values.size();
    m_values.append(EntryTypeTraits::create(value, std::forward<Arguments>(arguments)...));
    m_map[value] = newPosition;
    return true;
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
inline bool IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::remove(unsigned value)
{
    unsigned position = m_map[value];
    if (position >= m_values.size())
        return false;

    if (EntryTypeTraits::key(m_values[position]) == value) {
        EntryType lastValue = m_values.last();
        m_values[position] = WTFMove(lastValue);
        m_map[EntryTypeTraits::key(lastValue)] = position;
        m_values.removeLast();
        return true;
    }

    return false;
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
void IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::clear()
{
    m_values.shrink(0);
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
unsigned IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::size() const
{
    return m_values.size();
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
bool IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::isEmpty() const
{
    return !size();
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
bool IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::contains(unsigned value) const
{
    unsigned position = m_map[value];
    if (position >= m_values.size())
        return false;

    return EntryTypeTraits::key(m_values[position]) == value;
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
auto IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::get(unsigned value) -> EntryType*
{
    unsigned position = m_map[value];
    if (position >= m_values.size())
        return nullptr;

    EntryType& entry = m_values[position];
    if (EntryTypeTraits::key(entry) != value)
        return nullptr;
    
    return &entry;
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
auto IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::get(unsigned value) const -> const EntryType*
{
    return const_cast<IndexSparseSet*>(this)->get(value);
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
void IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::sort()
{
    std::sort(
        m_values.begin(), m_values.end(),
        [&] (const EntryType& a, const EntryType& b) {
            return EntryTypeTraits::key(a) < EntryTypeTraits::key(b);
        });

    // Bring m_map back in sync with m_values
    for (unsigned index = 0; index < m_values.size(); ++index) {
        unsigned key = EntryTypeTraits::key(m_values[index]);
        m_map[key] = index;
    }

#if ASSERT_ENABLED
    validate();
#endif
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
void IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::validate()
{
    RELEASE_ASSERT(m_values.size() < m_map.size());
    for (const EntryType& entry : *this)
        RELEASE_ASSERT(contains(EntryTypeTraits::key(entry)));
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
auto IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::begin() const -> const_iterator
{
    return m_values.begin();
}

template<typename EntryType, typename EntryTypeTraits, typename OverflowHandler>
auto IndexSparseSet<EntryType, EntryTypeTraits, OverflowHandler>::end() const -> const_iterator
{
    return m_values.end();
}

} // namespace WTF

using WTF::DefaultIndexSparseSetTraits;
using WTF::IndexSparseSet;
