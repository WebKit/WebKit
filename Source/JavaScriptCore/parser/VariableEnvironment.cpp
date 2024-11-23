/*
 * Copyright (C) 2015-2024 Apple Inc. All Rights Reserved.
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
#include <wtf/CommaPrinter.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/UniquedStringImpl.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CompactTDZEnvironment);
WTF_MAKE_TZONE_ALLOCATED_IMPL(VariableEnvironment);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(VariableEnvironment, RareData);

void VariableEnvironmentEntry::dump(PrintStream& out) const
{
    out.print(hex(m_bits));
}

VariableEnvironment& VariableEnvironment::operator=(const VariableEnvironment& other)
{
    VariableEnvironment env(other);
    swap(env);
    return *this;
}

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
    m_rareData.swap(other.m_rareData);
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

VariableEnvironment::Map::AddResult VariableEnvironment::declarePrivateField(const RefPtr<UniquedStringImpl>& identifier)
{
    getOrAddPrivateName(identifier.get());
    auto entry = VariableEnvironmentEntry();
    entry.setIsPrivateField();
    entry.setIsConst();
    entry.setIsCaptured();
    return m_map.add(identifier, entry);
}

VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declarePrivateAccessor(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry accessorTraits)
{
    if (!m_rareData)
        m_rareData = WTF::makeUnique<VariableEnvironment::RareData>();

    auto findResult = m_rareData->m_privateNames.find(identifier);

    if (findResult == m_rareData->m_privateNames.end()) {
        PrivateNameEntry meta(accessorTraits.bits());

        auto entry = VariableEnvironmentEntry();
        if (accessorTraits.isSetter())
            entry.setIsPrivateSetter();
        else {
            ASSERT(accessorTraits.isGetter());
            entry.setIsPrivateGetter();
        }
        entry.setIsConst();
        entry.setIsCaptured();
        m_map.add(identifier, entry);

        m_rareData->m_privateNames.add(identifier, meta);
        return PrivateDeclarationResult::Success;
    }

    PrivateNameEntry currentEntry = findResult->value;
    if ((accessorTraits.isSetter() && !currentEntry.isGetter())
        || (accessorTraits.isGetter() && !currentEntry.isSetter()))
        return PrivateDeclarationResult::DuplicatedName;

    if (accessorTraits.isStatic() != currentEntry.isStatic())
        return PrivateDeclarationResult::InvalidStaticNonStatic;

    PrivateNameEntry meta(currentEntry.bits() | accessorTraits.bits());
    m_rareData->m_privateNames.set(identifier, meta);

    auto entryIterator = m_map.find(identifier);
    ASSERT(entryIterator != m_map.end());
    if (accessorTraits.isSetter())
        entryIterator->value.setIsPrivateSetter();
    else {
        ASSERT(accessorTraits.isGetter());
        entryIterator->value.setIsPrivateGetter();
    }

    return PrivateDeclarationResult::Success;
}

VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declarePrivateSetter(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits modifierTraits)
{
    return declarePrivateAccessor(identifier, PrivateNameEntry(PrivateNameEntry::Traits::IsSetter | modifierTraits));
}

VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declarePrivateGetter(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits modifierTraits)
{
    return declarePrivateAccessor(identifier, PrivateNameEntry(PrivateNameEntry::Traits::IsGetter | modifierTraits));
}

bool VariableEnvironment::declarePrivateMethod(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits addionalTraits)
{
    if (!m_rareData)
        m_rareData = makeUnique<VariableEnvironment::RareData>();

    auto findResult = m_rareData->m_privateNames.find(identifier);

    if (findResult == m_rareData->m_privateNames.end()) {
        PrivateNameEntry meta(PrivateNameEntry::Traits::IsMethod | addionalTraits);

        auto entry = VariableEnvironmentEntry();
        entry.setIsPrivateMethod();
        entry.setIsConst();
        entry.setIsCaptured();
        m_map.add(identifier, entry);

        auto addResult = m_rareData->m_privateNames.add(identifier, meta);
        return addResult.isNewEntry;
    }

    return false; // Error: declaring a duplicate private name.
}

void VariableEnvironment::dump(PrintStream& out) const
{
    CommaPrinter comma(", "_s);
    for (auto& pair : m_map)
        out.print(comma, pair.key, " => "_s, pair.value);
}

void CompactTDZEnvironment::sortCompact(Compact& compact)
{
    std::sort(compact.begin(), compact.end(), [] (auto& a, auto& b) {
        return a.get() < b.get();
    });
}

CompactTDZEnvironment::CompactTDZEnvironment(const TDZEnvironment& env)
{
    m_hash = 0; // Note: XOR is commutative so order doesn't matter here.
    Compact variables = WTF::map(env, [this](auto& key) -> PackedRefPtr<UniquedStringImpl> {
        m_hash ^= key->hash();
        return key.get();
    });

    sortCompact(variables);
    m_variables = WTFMove(variables);
}

bool CompactTDZEnvironment::operator==(const CompactTDZEnvironment& other) const
{
    if (this == &other)
        return true;

    if (m_hash != other.m_hash)
        return false;

    auto equal = [&] (const Compact& compact, const Inflated& inflated) {
        if (compact.size() != inflated.size())
            return false;
        for (auto& ident : compact) {
            if (!inflated.contains(ident))
                return false;
        }
        return true;
    };

    bool result;
    WTF::switchOn(m_variables,
        [&] (const Compact& compact) {
            WTF::switchOn(other.m_variables,
                [&] (const Compact& otherCompact) {
                    result = compact == otherCompact;
                },
                [&] (const Inflated& otherInflated) {
                    result = equal(compact, otherInflated);
                });
        },
        [&] (const Inflated& inflated) {
            WTF::switchOn(other.m_variables,
                [&] (const Compact& otherCompact) {
                    result = equal(otherCompact, inflated);
                },
                [&] (const Inflated& otherInflated) {
                    result = inflated == otherInflated;
                });
        });

    return result;
}

TDZEnvironment& CompactTDZEnvironment::toTDZEnvironmentSlow() const
{
    Inflated inflated;
    {
        auto& compact = std::get<Compact>(m_variables);
        for (size_t i = 0; i < compact.size(); ++i) {
            auto addResult = inflated.add(compact[i]);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
    }
    m_variables = Variables(WTFMove(inflated));
    return const_cast<Inflated&>(std::get<Inflated>(m_variables));
}

CompactTDZEnvironmentMap::Handle CompactTDZEnvironmentMap::get(const TDZEnvironment& env)
{
    auto* environment = new CompactTDZEnvironment(env);
    bool isNewEntry;
    auto handle = get(environment, isNewEntry);
    if (!isNewEntry)
        delete environment;
    return handle;
}

CompactTDZEnvironmentMap::Handle CompactTDZEnvironmentMap::get(CompactTDZEnvironment* environment, bool& isNewEntry)
{
    CompactTDZEnvironmentKey key { *environment };
    auto addResult = m_map.add(key, 1);
    isNewEntry = addResult.isNewEntry;
    if (addResult.isNewEntry)
        return CompactTDZEnvironmentMap::Handle(*environment, *this);

    ++addResult.iterator->value;
    return CompactTDZEnvironmentMap::Handle(addResult.iterator->key.environment(), *this);
}

CompactTDZEnvironmentMap::Handle::~Handle()
{
    if (!m_map) {
        ASSERT(!m_environment);
        // This happens if we were moved into a different handle.
        return;
    }

    RELEASE_ASSERT(m_environment);
    auto iter = m_map->m_map.find(CompactTDZEnvironmentKey { *m_environment });
    RELEASE_ASSERT(iter != m_map->m_map.end());
    --iter->value;
    if (!iter->value) {
        ASSERT(m_environment == &iter->key.environment());
        m_map->m_map.remove(iter);
        delete m_environment;
    }
}

CompactTDZEnvironmentMap::Handle::Handle(const CompactTDZEnvironmentMap::Handle& other)
    : m_environment(other.m_environment)
    , m_map(other.m_map)
{
    if (m_map) {
        auto iter = m_map->m_map.find(CompactTDZEnvironmentKey { *m_environment });
        RELEASE_ASSERT(iter != m_map->m_map.end());
        ++iter->value;
    }
}

CompactTDZEnvironmentMap::Handle::Handle(CompactTDZEnvironment& environment, CompactTDZEnvironmentMap& map)
    : m_environment(&environment)
    , m_map(&map)
{ 
}

} // namespace JSC
