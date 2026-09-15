/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace Style {

// Side table for values that PrimitiveData refers to by handle rather than holding inline, which is
// what lets it stay packed. Each value type gets its own map.
template<typename ValueType> class ValueHandleMap {
public:
    static ValueHandleMap& NODELETE singleton()
    {
        static NeverDestroyed<ValueHandleMap> map;
        return map;
    }

    unsigned insert(Ref<ValueType>&&);
    void ref(unsigned handle);
    void deref(unsigned handle);

    ValueType& get(unsigned handle) const;

private:
    friend NeverDestroyed<ValueHandleMap>;
    ValueHandleMap() = default;

    struct Entry {
        uint64_t referenceCountMinusOne { 0 };
        RefPtr<ValueType> value;

        Entry() = default;
        Entry(Ref<ValueType>&& value)
            : value(WTF::move(value))
        {
        }
    };

    unsigned m_nextAvailableHandle { 1 };
    HashMap<unsigned, Entry> m_map;
};

template<typename ValueType> unsigned ValueHandleMap<ValueType>::insert(Ref<ValueType>&& value)
{
    ASSERT(m_nextAvailableHandle);

    Entry entry(WTF::move(value));

    // FIXME: This monotonically increasing handle generation scheme is potentially wasteful
    // of the handle space. Consider reusing empty handles. https://bugs.webkit.org/show_bug.cgi?id=80489
    while (!m_map.isValidKey(m_nextAvailableHandle) || !m_map.add(m_nextAvailableHandle, entry).isNewEntry)
        ++m_nextAvailableHandle;

    return m_nextAvailableHandle++;
}

template<typename ValueType> ValueType& ValueHandleMap<ValueType>::get(unsigned handle) const
{
    ASSERT(m_map.contains(handle));

    return *m_map.find(handle)->value.value;
}

template<typename ValueType> void ValueHandleMap<ValueType>::ref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    ++m_map.find(handle)->value.referenceCountMinusOne;
}

template<typename ValueType> void ValueHandleMap<ValueType>::deref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    auto it = m_map.find(handle);
    if (it->value.referenceCountMinusOne) {
        --it->value.referenceCountMinusOne;
        return;
    }

    m_map.remove(it);
}

} // namespace Style
} // namespace WebCore
