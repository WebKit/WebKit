/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/VariableEnvironment.h>

namespace JSC {

ALWAYS_INLINE VariableEnvironment::Map::AddResult VariableEnvironment::declarePrivateField(const Identifier& identifier)
{
    return declarePrivateField(identifier.impl());
}

inline bool VariableEnvironment::declarePrivateMethod(const Identifier& identifier)
{
    return declarePrivateMethod(identifier.impl());
}

inline bool VariableEnvironment::declareStaticPrivateMethod(const Identifier& identifier)
{
    return declarePrivateMethod(identifier.impl(), static_cast<PrivateNameEntry::Traits>(PrivateNameEntry::Traits::IsMethod | PrivateNameEntry::Traits::IsStatic));
}

inline VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declarePrivateSetter(const Identifier& identifier)
{
    return declarePrivateSetter(identifier.impl());
}

inline VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declareStaticPrivateSetter(const Identifier& identifier)
{
    return declarePrivateSetter(identifier.impl(), PrivateNameEntry::Traits::IsStatic);
}

inline VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declarePrivateGetter(const Identifier& identifier)
{
    return declarePrivateGetter(identifier.impl());
}

inline VariableEnvironment::PrivateDeclarationResult VariableEnvironment::declareStaticPrivateGetter(const Identifier& identifier)
{
    return declarePrivateGetter(identifier.impl(), PrivateNameEntry::Traits::IsStatic);
}

inline TDZEnvironment& CompactTDZEnvironment::toTDZEnvironment() const
{
    if (std::holds_alternative<Inflated>(m_variables))
        return const_cast<TDZEnvironment&>(std::get<Inflated>(m_variables));
    return toTDZEnvironmentSlow();
}

inline bool TDZEnvironmentLink::contains(UniquedStringImpl* impl) const
{
    return m_handle.environment().toTDZEnvironment().contains(impl);
}

inline TDZEnvironmentLink::TDZEnvironmentLink(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent)
    : m_handle(WTF::move(handle))
    , m_parent(WTF::move(parent))
{
}

inline RefPtr<TDZEnvironmentLink> TDZEnvironmentLink::create(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent)
{
    return adoptRef(new TDZEnvironmentLink(WTF::move(handle), WTF::move(parent)));
}

inline VariableEnvironment::VariableEnvironment(const VariableEnvironment& other)
    : m_map(other.m_map)
    , m_isEverythingCaptured(other.m_isEverythingCaptured)
    , m_hasAwaitUsingDeclaration(other.m_hasAwaitUsingDeclaration)
    , m_rareData(other.m_rareData ? WTF::makeUnique<VariableEnvironment::RareData>(*other.m_rareData) : nullptr)
{
}

ALWAYS_INLINE PrivateNameEnvironment::AddResult VariableEnvironment::addPrivateName(const Identifier& identifier)
{
    return addPrivateName(identifier.impl());
}

ALWAYS_INLINE PrivateNameEnvironment::AddResult VariableEnvironment::addPrivateName(const RefPtr<UniquedStringImpl>& identifier)
{
    if (!m_rareData)
        m_rareData = makeUnique<VariableEnvironment::RareData>();

    return m_rareData->m_privateNames.add(identifier, PrivateNameEntry());
}

ALWAYS_INLINE void VariableEnvironment::addPrivateNamesFrom(const PrivateNameEnvironment* privateNameEnvironment)
{
    if (!privateNameEnvironment)
        return;

    if (!m_rareData)
        m_rareData = makeUnique<VariableEnvironment::RareData>();

    for (auto entry : *privateNameEnvironment)
        m_rareData->m_privateNames.add(entry.key, entry.value);
}

inline PrivateNameEntry& VariableEnvironment::getOrAddPrivateName(UniquedStringImpl* impl)
{
    if (!m_rareData)
        m_rareData = WTF::makeUnique<VariableEnvironment::RareData>();

    return m_rareData->m_privateNames.add(impl, PrivateNameEntry()).iterator->value;
}

inline bool CompactTDZEnvironmentKey::equal(const CompactTDZEnvironmentKey& a, const CompactTDZEnvironmentKey& b)
{
    return *a.m_environment == *b.m_environment;
}

inline CompactTDZEnvironmentMap::Handle& CompactTDZEnvironmentMap::Handle::operator=(Handle&& other)
{
    Handle handle(WTF::move(other));
    swap(handle);
    return *this;
}

inline CompactTDZEnvironmentMap::Handle& CompactTDZEnvironmentMap::Handle::operator=(const Handle& other)
{
    Handle handle(other);
    swap(handle);
    return *this;
}

} // namespace JSC
