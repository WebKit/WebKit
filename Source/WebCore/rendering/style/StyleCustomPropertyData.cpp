/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleCustomPropertyData.h"

namespace WebCore {

StyleCustomPropertyData::StyleCustomPropertyData(const StyleCustomPropertyData& other)
    : RefCounted()
{
    // Don't reference `other` if it already has a parent of its own, to avoid
    // creating a linked list of StyleCustomPropertyData objects.
    bool shouldReferenceAsParentValues = !other.m_parentValues && !other.m_ownValues.isEmpty();

    if (shouldReferenceAsParentValues) {
        m_parentValues = &other;
        m_ownValuesSizeExcludingOverriddenParentValues = 0;
    } else {
        m_parentValues = other.m_parentValues.get();
        m_ownValues = other.m_ownValues;
        m_ownValuesSizeExcludingOverriddenParentValues = other.m_ownValuesSizeExcludingOverriddenParentValues;
    }
}

const CSSCustomPropertyValue* StyleCustomPropertyData::get(const AtomString& name) const
{
    if (auto* value = m_ownValues.get(name))
        return value;
    if (m_parentValues) {
        ASSERT(!m_parentValues->m_parentValues);
        if (auto* value = m_parentValues->m_ownValues.get(name))
            return value;
    }
    return nullptr;
}

void StyleCustomPropertyData::set(const AtomString& name, Ref<const CSSCustomPropertyValue>&& value)
{
    auto addResult = m_ownValues.set(name, WTFMove(value));
    if (addResult.isNewEntry && (!m_parentValues || !m_parentValues->m_ownValues.contains(name)))
        ++m_ownValuesSizeExcludingOverriddenParentValues;
}

bool StyleCustomPropertyData::operator==(const StyleCustomPropertyData& other) const
{
    if (size() != other.size())
        return false;

    for (auto& entry : m_ownValues) {
        auto* otherValue = other.get(entry.key);
        if (!otherValue || !entry.value->equals(*otherValue))
            return false;
    }

    if (m_parentValues) {
        ASSERT(!m_parentValues->m_parentValues);
        for (auto& entry : m_parentValues->m_ownValues) {
            if (m_ownValues.contains(entry.key))
                continue;
            auto* otherValue = other.get(entry.key);
            if (!otherValue || !entry.value->equals(*otherValue))
                return false;
        }
    }

    return true;
}

void StyleCustomPropertyData::forEach(const Function<void(const KeyValuePair<AtomString, RefPtr<const CSSCustomPropertyValue>>&)>& callback) const
{
    if (m_parentValues) {
        ASSERT(!m_parentValues->m_parentValues);
        for (auto& entry : m_parentValues->m_ownValues) {
            if (!m_ownValues.contains(entry.key))
                callback(entry);
        }
    }
    for (auto& entry : m_ownValues)
        callback(entry);
}

AtomString StyleCustomPropertyData::findKeyAtIndex(unsigned index) const
{
    unsigned currentIndex = 0;
    if (m_parentValues) {
        ASSERT(!m_parentValues->m_parentValues);
        for (auto& key : m_parentValues->m_ownValues.keys()) {
            if (!m_ownValues.contains(key)) {
                if (currentIndex == index)
                    return key;
                ++currentIndex;
            }
        }
    }
    for (auto& key : m_ownValues.keys()) {
        if (currentIndex == index)
            return key;
        ++currentIndex;
    }
    return { };
}

unsigned StyleCustomPropertyData::size() const
{
    return m_ownValuesSizeExcludingOverriddenParentValues + (m_parentValues ? m_parentValues->m_ownValuesSizeExcludingOverriddenParentValues : 0); 
}

} // namespace WebCore
