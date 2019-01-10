/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
#include "VariableEnvironment.h"
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

void VariableEnvironment::markVariableAsCapturedIfDefined(const RefPtr<UniquedStringImpl>& identifier)
{
    auto findResult = m_map.find(identifier);
    if (findResult != m_map.end())
        findResult->value.setIsCaptured();
}

void VariableEnvironment::markVariableAsCaptured(const RefPtr<UniquedStringImpl>& identifier)
{
    auto findResult = m_map.find(identifier);
    RELEASE_ASSERT(findResult != m_map.end());
    findResult->value.setIsCaptured();
}

void VariableEnvironment::markAllVariablesAsCaptured()
{
    if (m_isEverythingCaptured)
        return;

    m_isEverythingCaptured = true; // For fast queries.
    // We must mark every entry as captured for when we iterate through m_map and entry.isCaptured() is called.
    for (auto& value : m_map.values())
        value.setIsCaptured();
}

bool VariableEnvironment::hasCapturedVariables() const
{
    if (m_isEverythingCaptured)
        return size() > 0;
    for (auto& value : m_map.values()) {
        if (value.isCaptured())
            return true;
    }
    return false;
}

bool VariableEnvironment::captures(UniquedStringImpl* identifier) const
{
    if (m_isEverythingCaptured)
        return true;

    auto findResult = m_map.find(identifier);
    if (findResult == m_map.end())
        return false;
    return findResult->value.isCaptured();
}

void VariableEnvironment::swap(VariableEnvironment& other)
{
    m_map.swap(other.m_map);
    m_isEverythingCaptured = other.m_isEverythingCaptured;
}

void VariableEnvironment::markVariableAsImported(const RefPtr<UniquedStringImpl>& identifier)
{
    auto findResult = m_map.find(identifier);
    RELEASE_ASSERT(findResult != m_map.end());
    findResult->value.setIsImported();
}

void VariableEnvironment::markVariableAsExported(const RefPtr<UniquedStringImpl>& identifier)
{
    auto findResult = m_map.find(identifier);
    RELEASE_ASSERT(findResult != m_map.end());
    findResult->value.setIsExported();
}

CompactVariableEnvironment::CompactVariableEnvironment(const VariableEnvironment& env)
    : m_isEverythingCaptured(env.isEverythingCaptured())
{
    Vector<std::pair<UniquedStringImpl*, VariableEnvironmentEntry>, 32> sortedEntries;
    sortedEntries.reserveInitialCapacity(env.size());
    for (auto& pair : env)
        sortedEntries.append({ pair.key.get(), pair.value });

    std::sort(sortedEntries.begin(), sortedEntries.end(), [] (const auto& a, const auto& b) {
        return a.first < b.first;
    });

    m_hash = 0;
    m_variables.reserveInitialCapacity(sortedEntries.size());
    m_variableMetadata.reserveInitialCapacity(sortedEntries.size());
    for (const auto& pair : sortedEntries) {
        m_variables.append(pair.first);
        m_variableMetadata.append(pair.second);
        m_hash ^= pair.first->hash();
        m_hash += pair.second.bits();
    }

    if (m_isEverythingCaptured)
        m_hash *= 2;
}

bool CompactVariableEnvironment::operator==(const CompactVariableEnvironment& other) const
{
    if (this == &other)
        return true;
    if (m_isEverythingCaptured != other.m_isEverythingCaptured)
        return false;
    if (m_variables != other.m_variables)
        return false;
    if (m_variableMetadata != other.m_variableMetadata)
        return false;
    return true;
}

VariableEnvironment CompactVariableEnvironment::toVariableEnvironment() const
{
    VariableEnvironment result;
    ASSERT(m_variables.size() == m_variableMetadata.size());
    for (size_t i = 0; i < m_variables.size(); ++i) {
        auto addResult = result.add(m_variables[i]);
        ASSERT(addResult.isNewEntry);
        addResult.iterator->value = m_variableMetadata[i];
    }

    if (m_isEverythingCaptured)
        result.markAllVariablesAsCaptured();

    return result;
}

CompactVariableMap::Handle CompactVariableMap::get(const VariableEnvironment& env)
{
    auto* environment = new CompactVariableEnvironment(env);
    CompactVariableMapKey key { *environment };
    auto addResult = m_map.add(key, 1);
    if (addResult.isNewEntry)
        return CompactVariableMap::Handle(*environment, *this);

    delete environment;
    ++addResult.iterator->value;
    return CompactVariableMap::Handle(addResult.iterator->key.environment(), *this);
}

CompactVariableMap::Handle::~Handle()
{
    if (!m_map) {
        ASSERT(!m_environment);
        // This happens if we were moved into a different handle.
        return;
    }

    RELEASE_ASSERT(m_environment);
    auto iter = m_map->m_map.find(CompactVariableMapKey { *m_environment });
    RELEASE_ASSERT(iter != m_map->m_map.end());
    --iter->value;
    if (!iter->value) {
        ASSERT(m_environment == &iter->key.environment());
        m_map->m_map.remove(iter);
        delete m_environment;
    }
}

} // namespace JSC
