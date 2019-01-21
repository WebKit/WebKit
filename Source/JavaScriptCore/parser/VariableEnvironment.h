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

#pragma once

#include "Identifier.h"
#include <wtf/HashMap.h>

namespace JSC {

struct VariableEnvironmentEntry {
public:
    ALWAYS_INLINE bool isCaptured() const { return m_bits & IsCaptured; }
    ALWAYS_INLINE bool isConst() const { return m_bits & IsConst; }
    ALWAYS_INLINE bool isVar() const { return m_bits & IsVar; }
    ALWAYS_INLINE bool isLet() const { return m_bits & IsLet; }
    ALWAYS_INLINE bool isExported() const { return m_bits & IsExported; }
    ALWAYS_INLINE bool isImported() const { return m_bits & IsImported; }
    ALWAYS_INLINE bool isImportedNamespace() const { return m_bits & IsImportedNamespace; }
    ALWAYS_INLINE bool isFunction() const { return m_bits & IsFunction; }
    ALWAYS_INLINE bool isParameter() const { return m_bits & IsParameter; }
    ALWAYS_INLINE bool isSloppyModeHoistingCandidate() const { return m_bits & IsSloppyModeHoistingCandidate; }

    ALWAYS_INLINE void setIsCaptured() { m_bits |= IsCaptured; }
    ALWAYS_INLINE void setIsConst() { m_bits |= IsConst; }
    ALWAYS_INLINE void setIsVar() { m_bits |= IsVar; }
    ALWAYS_INLINE void setIsLet() { m_bits |= IsLet; }
    ALWAYS_INLINE void setIsExported() { m_bits |= IsExported; }
    ALWAYS_INLINE void setIsImported() { m_bits |= IsImported; }
    ALWAYS_INLINE void setIsImportedNamespace() { m_bits |= IsImportedNamespace; }
    ALWAYS_INLINE void setIsFunction() { m_bits |= IsFunction; }
    ALWAYS_INLINE void setIsParameter() { m_bits |= IsParameter; }
    ALWAYS_INLINE void setIsSloppyModeHoistingCandidate() { m_bits |= IsSloppyModeHoistingCandidate; }

    ALWAYS_INLINE void clearIsVar() { m_bits &= ~IsVar; }

    uint16_t bits() const { return m_bits; }

    bool operator==(const VariableEnvironmentEntry& other) const
    {
        return m_bits == other.m_bits;
    }

private:
    enum Traits : uint16_t {
        IsCaptured = 1 << 0,
        IsConst = 1 << 1,
        IsVar = 1 << 2,
        IsLet = 1 << 3,
        IsExported = 1 << 4,
        IsImported = 1 << 5,
        IsImportedNamespace = 1 << 6,
        IsFunction = 1 << 7,
        IsParameter = 1 << 8,
        IsSloppyModeHoistingCandidate = 1 << 9
    };
    uint16_t m_bits { 0 };
};

struct VariableEnvironmentEntryHashTraits : HashTraits<VariableEnvironmentEntry> {
    static const bool needsDestruction = false;
};

class VariableEnvironment {
private:
    typedef HashMap<RefPtr<UniquedStringImpl>, VariableEnvironmentEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, VariableEnvironmentEntryHashTraits> Map;
public:
    VariableEnvironment() = default;
    VariableEnvironment(VariableEnvironment&& other) = default;
    VariableEnvironment(const VariableEnvironment&) = default;
    VariableEnvironment& operator=(const VariableEnvironment&) = default;
    VariableEnvironment& operator=(VariableEnvironment&&) = default;

    ALWAYS_INLINE Map::iterator begin() { return m_map.begin(); }
    ALWAYS_INLINE Map::iterator end() { return m_map.end(); }
    ALWAYS_INLINE Map::const_iterator begin() const { return m_map.begin(); }
    ALWAYS_INLINE Map::const_iterator end() const { return m_map.end(); }
    ALWAYS_INLINE Map::AddResult add(const RefPtr<UniquedStringImpl>& identifier) { return m_map.add(identifier, VariableEnvironmentEntry()); }
    ALWAYS_INLINE Map::AddResult add(const Identifier& identifier) { return add(identifier.impl()); }
    ALWAYS_INLINE unsigned size() const { return m_map.size(); }
    ALWAYS_INLINE bool contains(const RefPtr<UniquedStringImpl>& identifier) const { return m_map.contains(identifier); }
    ALWAYS_INLINE bool remove(const RefPtr<UniquedStringImpl>& identifier) { return m_map.remove(identifier); }
    ALWAYS_INLINE Map::iterator find(const RefPtr<UniquedStringImpl>& identifier) { return m_map.find(identifier); }
    ALWAYS_INLINE Map::const_iterator find(const RefPtr<UniquedStringImpl>& identifier) const { return m_map.find(identifier); }
    void swap(VariableEnvironment& other);
    void markVariableAsCapturedIfDefined(const RefPtr<UniquedStringImpl>& identifier);
    void markVariableAsCaptured(const RefPtr<UniquedStringImpl>& identifier);
    void markAllVariablesAsCaptured();
    bool hasCapturedVariables() const;
    bool captures(UniquedStringImpl* identifier) const;
    void markVariableAsImported(const RefPtr<UniquedStringImpl>& identifier);
    void markVariableAsExported(const RefPtr<UniquedStringImpl>& identifier);

    bool isEverythingCaptured() const { return m_isEverythingCaptured; }

private:
    Map m_map;
    bool m_isEverythingCaptured { false };
};

class CompactVariableEnvironment {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CompactVariableEnvironment);
public:
    CompactVariableEnvironment(const VariableEnvironment&);
    VariableEnvironment toVariableEnvironment() const;

    bool operator==(const CompactVariableEnvironment&) const;
    unsigned hash() const { return m_hash; }

private:
    Vector<RefPtr<UniquedStringImpl>> m_variables;
    Vector<VariableEnvironmentEntry> m_variableMetadata;
    unsigned m_hash;
    bool m_isEverythingCaptured;
};

struct CompactVariableMapKey {
    CompactVariableMapKey()
        : m_environment(nullptr)
    {
        ASSERT(isHashTableEmptyValue());
    }

    CompactVariableMapKey(CompactVariableEnvironment& environment)
        : m_environment(&environment)
    { }

    static unsigned hash(const CompactVariableMapKey& key) { return key.m_environment->hash(); }
    static bool equal(const CompactVariableMapKey& a, const CompactVariableMapKey& b) { return *a.m_environment == *b.m_environment; }
    static const bool safeToCompareToEmptyOrDeleted = false;
    static void makeDeletedValue(CompactVariableMapKey& key)
    {
        key.m_environment = reinterpret_cast<CompactVariableEnvironment*>(1);
    }
    bool isHashTableDeletedValue() const
    {
        return m_environment == reinterpret_cast<CompactVariableEnvironment*>(1);
    }
    bool isHashTableEmptyValue() const
    {
        return !m_environment;
    }

    CompactVariableEnvironment& environment()
    {
        RELEASE_ASSERT(!isHashTableDeletedValue());
        RELEASE_ASSERT(!isHashTableEmptyValue());
        return *m_environment;
    }

private:
    CompactVariableEnvironment* m_environment;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::CompactVariableMapKey> {
    using Hash = JSC::CompactVariableMapKey;
};

template<> struct HashTraits<JSC::CompactVariableMapKey> : GenericHashTraits<JSC::CompactVariableMapKey> {
    static const bool emptyValueIsZero = true;
    static JSC::CompactVariableMapKey emptyValue() { return JSC::CompactVariableMapKey(); }

    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(JSC::CompactVariableMapKey key) { return key.isHashTableEmptyValue(); }

    static void constructDeletedValue(JSC::CompactVariableMapKey& key) { JSC::CompactVariableMapKey::makeDeletedValue(key); }
    static bool isDeletedValue(JSC::CompactVariableMapKey key) { return key.isHashTableDeletedValue(); }
};

} // namespace WTF

namespace JSC {

class CompactVariableMap : public RefCounted<CompactVariableMap> {
public:
    class Handle {
        WTF_MAKE_NONCOPYABLE(Handle); // If we wanted to make this copyable, we'd need to do a hashtable lookup and bump the reference count of the map entry.
    public:
        Handle(CompactVariableEnvironment& environment, CompactVariableMap& map)
            : m_environment(&environment)
            , m_map(&map)
        { }
        Handle(Handle&& other)
            : m_environment(other.m_environment)
            , m_map(WTFMove(other.m_map))
        {
            RELEASE_ASSERT(!!m_environment == !!m_map);
            ASSERT(!other.m_map);
            other.m_environment = nullptr;
        }
        ~Handle();

        const CompactVariableEnvironment& environment() const
        {
            return *m_environment;
        }

    private:
        CompactVariableEnvironment* m_environment;
        RefPtr<CompactVariableMap> m_map;
    };

    Handle get(const VariableEnvironment&);

private:
    friend class Handle;
    HashMap<CompactVariableMapKey, unsigned> m_map;
};

} // namespace JSC
