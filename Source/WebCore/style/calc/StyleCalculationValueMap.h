/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

#include "StyleCalculationValue.h"
#include <wtf/HashMap.h>

namespace WebCore {
namespace Style {
namespace Calculation {

class ValueMap {
public:
    static ValueMap& NODELETE calculationValues();

    unsigned insert(Ref<Value>&&);
    void ref(unsigned handle);
    void deref(unsigned handle);

    Value& get(unsigned handle) const;

private:
    friend NeverDestroyed<ValueMap>;
    ValueMap();

    struct Entry {
        uint64_t referenceCountMinusOne { 0 };
        RefPtr<Value> value;
        Entry() = default;
        Entry(Ref<Value>&&);
    };

    unsigned m_nextAvailableHandle { 1 };
    HashMap<unsigned, Entry> m_map;
};

inline ValueMap::Entry::Entry(Ref<Value>&& value)
    : value(WTF::move(value))
{
}

inline unsigned ValueMap::insert(Ref<Value>&& value)
{
    ASSERT(m_nextAvailableHandle);

    Entry entry(WTF::move(value));

    // FIXME: This monotonically increasing handle generation scheme is potentially wasteful
    // of the handle space. Consider reusing empty handles. https://bugs.webkit.org/show_bug.cgi?id=80489
    while (!m_map.isValidKey(m_nextAvailableHandle) || !m_map.add(m_nextAvailableHandle, entry).isNewEntry)
        ++m_nextAvailableHandle;

    return m_nextAvailableHandle++;
}

inline Value& ValueMap::get(unsigned handle) const
{
    ASSERT(m_map.contains(handle));

    return *m_map.find(handle)->value.value;
}

inline void ValueMap::ref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    ++m_map.find(handle)->value.referenceCountMinusOne;
}

inline void ValueMap::deref(unsigned handle)
{
    ASSERT(m_map.contains(handle));

    auto it = m_map.find(handle);
    if (it->value.referenceCountMinusOne) {
        --it->value.referenceCountMinusOne;
        return;
    }

    m_map.remove(it);
}

} // namespace Calculation
} // namespace Style
} // namespace WebCore
