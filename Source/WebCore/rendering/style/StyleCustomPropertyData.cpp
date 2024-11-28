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

static constexpr auto maximumAncestorCount = 4;

StyleCustomPropertyData::StyleCustomPropertyData(const StyleCustomPropertyData& other)
    : m_size(other.m_size)
    , m_mayHaveAnimatableProperties(other.m_mayHaveAnimatableProperties)
{
    auto shouldReferenceAsParentValues = [&] {
        // Always reference the root style since it likely gets shared a lot.
        if (!other.m_parentValues && !other.m_ownValues.isEmpty())
            return true;

        // Limit the list length.
        if (other.m_ancestorCount >= maximumAncestorCount)
            return false;

        // If the number of properties is small we just copy them to avoid creating unnecessarily long linked lists.
        static constexpr auto parentReferencePropertyCountThreshold = 8;
        return other.m_ownValues.size() > parentReferencePropertyCountThreshold;
    }();

    // If there are mutations on multiple levels this constructs a linked list of property data objects.
    if (shouldReferenceAsParentValues)
        m_parentValues = &other;
    else {
        m_parentValues = other.m_parentValues;
        m_ownValues = other.m_ownValues;
    }

    if (m_parentValues)
        m_ancestorCount = m_parentValues->m_ancestorCount + 1;

#if ASSERT_ENABLED
    if (m_parentValues)
        m_parentValues->m_hasChildren = true;
#endif
}

const CSSCustomPropertyValue* StyleCustomPropertyData::get(const AtomString& name) const
{
    for (auto* propertyData = this; propertyData; propertyData = propertyData->m_parentValues.get()) {
        if (auto* value = propertyData->m_ownValues.get(name))
            return value;
    }
    return nullptr;
}

void StyleCustomPropertyData::set(const AtomString& name, Ref<const CSSCustomPropertyValue>&& value)
{
    ASSERT(!m_hasChildren);
    ASSERT([&] {
        auto* existing = get(name);
        return !existing || !existing->equals(value);
    }());

    m_mayHaveAnimatableProperties = m_mayHaveAnimatableProperties || value->isAnimatable();

    auto addResult = m_ownValues.set(name, WTFMove(value));

    bool isNewProperty = addResult.isNewEntry && (!m_parentValues || !m_parentValues->get(name));
    if (isNewProperty)
        ++m_size;
}

bool StyleCustomPropertyData::operator==(const StyleCustomPropertyData& other) const
{
    if (m_size != other.m_size)
        return false;

    if (m_parentValues == other.m_parentValues) {
        // This relies on the values in m_ownValues never being equal to those in m_parentValues.
        if (m_ownValues.size() != other.m_ownValues.size())
            return false;

        for (auto& entry : m_ownValues) {
            auto* otherValue = other.m_ownValues.get(entry.key);
            if (!otherValue || !entry.value->equals(*otherValue))
                return false;
        }
        return true;
    }

    bool isEqual = true;
    forEachInternal([&](auto& entry) {
        auto* otherValue = other.get(entry.key);
        if (!otherValue || !entry.value->equals(*otherValue)) {
            isEqual = false;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });

    return isEqual;
}

template<typename Callback>
void StyleCustomPropertyData::forEachInternal(Callback&& callback) const
{
    Vector<const StyleCustomPropertyData*, maximumAncestorCount> descendants;

    auto isOverridenByDescendants = [&](auto& key) {
        for (auto* descendant : descendants) {
            if (descendant->m_ownValues.contains(key))
                return true;
        }
        return false;
    };

    auto* propertyData = this;
    while (true) {
        for (auto& entry : propertyData->m_ownValues) {
            if (isOverridenByDescendants(entry.key))
                continue;
            auto status = callback(entry);
            if (status == IterationStatus::Done)
                return;
        }
        if (!propertyData->m_parentValues)
            return;
        descendants.append(propertyData);
        propertyData = propertyData->m_parentValues.get();
    }
}

void StyleCustomPropertyData::forEach(const Function<IterationStatus(const KeyValuePair<AtomString, RefPtr<const CSSCustomPropertyValue>>&)>& callback) const
{
    forEachInternal(callback);
}

AtomString StyleCustomPropertyData::findKeyAtIndex(unsigned index) const
{
    unsigned currentIndex = 0;
    AtomString key;
    forEachInternal([&](auto& entry) {
        if (currentIndex == index) {
            key = entry.key;
            return IterationStatus::Done;
        }
        ++currentIndex;
        return IterationStatus::Continue;
    });
    return key;
}

#if !LOG_DISABLED
void StyleCustomPropertyData::dumpDifferences(TextStream& ts, const StyleCustomPropertyData& other) const
{
    if (*this != other)
        ts << "custom properies differ\n";
}
#endif // !LOG_DISABLED

} // namespace WebCore
