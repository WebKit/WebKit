/*
 * Copyright (C) 2015-2019 Apple Inc. All Rights Reserved.
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
#include <wtf/HashSet.h>
#include <wtf/IteratorRange.h>
#include <wtf/Variant.h>

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
    ALWAYS_INLINE bool isPrivateName() const { return m_bits & IsPrivateName; }

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
    ALWAYS_INLINE void setIsPrivateName() { m_bits |= IsPrivateName; }

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
        IsSloppyModeHoistingCandidate = 1 << 9,
        IsPrivateName = 1 << 10,
    };
    uint16_t m_bits { 0 };
};

struct VariableEnvironmentEntryHashTraits : HashTraits<VariableEnvironmentEntry> {
    static constexpr bool needsDestruction = false;
};

struct PrivateNameEntry {
public:
    PrivateNameEntry(uint16_t traits = 0) { m_bits = traits; }

    ALWAYS_INLINE bool isUsed() const { return m_bits & IsUsed; }
    ALWAYS_INLINE bool isDeclared() const { return m_bits & IsDeclared; }

    ALWAYS_INLINE void setIsUsed() { m_bits |= IsUsed; }
    ALWAYS_INLINE void setIsDeclared() { m_bits |= IsDeclared; }

    uint16_t bits() const { return m_bits; }

    bool operator==(const PrivateNameEntry& other) const
    {
        return m_bits == other.m_bits;
    }

    enum Traits : uint16_t {
        IsUsed = 1 << 0,
        IsDeclared = 1 << 1,
    };

private:
    uint16_t m_bits { 0 };
};

struct PrivateNameEntryHashTraits : HashTraits<PrivateNameEntry> {
    static const bool needsDestruction = false;
};

class VariableEnvironment {
    WTF_MAKE_FAST_ALLOCATED;
private:
    typedef HashMap<PackedRefPtr<UniquedStringImpl>, VariableEnvironmentEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, VariableEnvironmentEntryHashTraits> Map;
    typedef HashMap<PackedRefPtr<UniquedStringImpl>, PrivateNameEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, PrivateNameEntryHashTraits> PrivateNames;
public:
    VariableEnvironment() { }
    VariableEnvironment(VariableEnvironment&& other)
        : m_map(WTFMove(other.m_map))
        , m_isEverythingCaptured(other.m_isEverythingCaptured)
        , m_rareData(WTFMove(other.m_rareData))
    {
    }
    VariableEnvironment(const VariableEnvironment& other)
        : m_map(other.m_map)
        , m_isEverythingCaptured(other.m_isEverythingCaptured)
        , m_rareData(other.m_rareData ? WTF::makeUnique<VariableEnvironment::RareData>(*other.m_rareData) : nullptr)
    {
    }
    VariableEnvironment& operator=(const VariableEnvironment& other);

    ALWAYS_INLINE Map::iterator begin() { return m_map.begin(); }
    ALWAYS_INLINE Map::iterator end() { return m_map.end(); }
    ALWAYS_INLINE Map::const_iterator begin() const { return m_map.begin(); }
    ALWAYS_INLINE Map::const_iterator end() const { return m_map.end(); }
    ALWAYS_INLINE Map::AddResult add(const RefPtr<UniquedStringImpl>& identifier) { return m_map.add(identifier, VariableEnvironmentEntry()); }
    ALWAYS_INLINE Map::AddResult add(const Identifier& identifier) { return add(identifier.impl()); }
    ALWAYS_INLINE unsigned size() const { return m_map.size() + privateNamesSize(); }
    ALWAYS_INLINE unsigned mapSize() const { return m_map.size(); }
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
    bool isEmpty() const { return !m_map.size(); }

    using PrivateNamesRange = WTF::IteratorRange<PrivateNames::iterator>;

    ALWAYS_INLINE Map::AddResult declarePrivateName(const Identifier& identifier) { return declarePrivateName(identifier.impl()); }
    ALWAYS_INLINE void usePrivateName(const Identifier& identifier) { usePrivateName(identifier.impl()); }

    Map::AddResult declarePrivateName(const RefPtr<UniquedStringImpl>& identifier)
    {
        auto& meta = getOrAddPrivateName(identifier.get());
        meta.setIsDeclared();
        auto entry = VariableEnvironmentEntry();
        entry.setIsPrivateName();
        entry.setIsConst();
        entry.setIsCaptured();
        return m_map.add(identifier, entry);
    }
    void usePrivateName(const RefPtr<UniquedStringImpl>& identifier)
    {
        auto& meta = getOrAddPrivateName(identifier.get());
        meta.setIsUsed();
        if (meta.isDeclared())
            find(identifier)->value.setIsCaptured();
    }

    ALWAYS_INLINE PrivateNamesRange privateNames() const
    {
        // Use of the IteratorRange must be guarded to prevent ASSERT failures in checkValidity().
        ASSERT(privateNamesSize() > 0);
        return makeIteratorRange(m_rareData->m_privateNames.begin(), m_rareData->m_privateNames.end());
    }

    ALWAYS_INLINE unsigned privateNamesSize() const
    {
        if (!m_rareData)
            return 0;
        return m_rareData->m_privateNames.size();
    }

    ALWAYS_INLINE bool hasPrivateName(const Identifier& identifier)
    {
        if (!m_rareData)
            return false;
        return m_rareData->m_privateNames.contains(identifier.impl());
    }

    ALWAYS_INLINE void copyPrivateNamesTo(VariableEnvironment& other) const
    {
        if (!m_rareData)
            return;
        if (!other.m_rareData)
            other.m_rareData = WTF::makeUnique<VariableEnvironment::RareData>();
        if (privateNamesSize() > 0) {
            for (auto entry : privateNames()) {
                if (!(entry.value.isUsed() && entry.value.isDeclared()))
                    other.m_rareData->m_privateNames.add(entry.key, entry.value);
            }
        }
    }

    ALWAYS_INLINE void copyUndeclaredPrivateNamesTo(VariableEnvironment& outer) const {
        // Used by the Parser to transfer recorded uses of PrivateNames from an
        // inner PrivateNameEnvironment into an outer one, in case a PNE is used
        // earlier in the source code than it is defined.
        if (privateNamesSize() > 0) {
            for (auto entry : privateNames()) {
                if (entry.value.isUsed() && !entry.value.isDeclared())
                    outer.getOrAddPrivateName(entry.key.get()).setIsUsed();
            }
        }
    }

    struct RareData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        RareData() { }
        RareData(RareData&& other)
            : m_privateNames(WTFMove(other.m_privateNames))
        {
        }
        RareData(const RareData&) = default;
        RareData& operator=(const RareData&) = default;
        PrivateNames m_privateNames;
    };

private:
    friend class CachedVariableEnvironment;

    Map m_map;
    bool m_isEverythingCaptured { false };

    PrivateNameEntry& getOrAddPrivateName(UniquedStringImpl* impl)
    {
        if (!m_rareData)
            m_rareData = WTF::makeUnique<VariableEnvironment::RareData>();

        return m_rareData->m_privateNames.add(impl, PrivateNameEntry()).iterator->value;
    }

    std::unique_ptr<VariableEnvironment::RareData> m_rareData;
};

using TDZEnvironment = HashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash>;

class CompactTDZEnvironment {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(CompactTDZEnvironment);

    friend class CachedCompactTDZEnvironment;

    using Compact = Vector<PackedRefPtr<UniquedStringImpl>>;
    using Inflated = TDZEnvironment;
    using Variables = Variant<Compact, Inflated>;

public:
    CompactTDZEnvironment(const TDZEnvironment&);

    bool operator==(const CompactTDZEnvironment&) const;
    unsigned hash() const { return m_hash; }

    static void sortCompact(Compact&);

    TDZEnvironment& toTDZEnvironment() const
    {
        if (WTF::holds_alternative<Inflated>(m_variables))
            return const_cast<TDZEnvironment&>(WTF::get<Inflated>(m_variables));
        return toTDZEnvironmentSlow();
    }

private:
    CompactTDZEnvironment() = default;
    TDZEnvironment& toTDZEnvironmentSlow() const;

    mutable Variables m_variables;
    unsigned m_hash;
};

struct CompactTDZEnvironmentKey {
    CompactTDZEnvironmentKey()
        : m_environment(nullptr)
    {
        ASSERT(isHashTableEmptyValue());
    }

    CompactTDZEnvironmentKey(CompactTDZEnvironment& environment)
        : m_environment(&environment)
    { }

    static unsigned hash(const CompactTDZEnvironmentKey& key) { return key.m_environment->hash(); }
    static bool equal(const CompactTDZEnvironmentKey& a, const CompactTDZEnvironmentKey& b) { return *a.m_environment == *b.m_environment; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    static void makeDeletedValue(CompactTDZEnvironmentKey& key)
    {
        key.m_environment = reinterpret_cast<CompactTDZEnvironment*>(1);
    }
    bool isHashTableDeletedValue() const
    {
        return m_environment == reinterpret_cast<CompactTDZEnvironment*>(1);
    }
    bool isHashTableEmptyValue() const
    {
        return !m_environment;
    }

    CompactTDZEnvironment& environment()
    {
        RELEASE_ASSERT(!isHashTableDeletedValue());
        RELEASE_ASSERT(!isHashTableEmptyValue());
        return *m_environment;
    }

private:
    CompactTDZEnvironment* m_environment;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::CompactTDZEnvironmentKey> : JSC::CompactTDZEnvironmentKey { };

template<> struct HashTraits<JSC::CompactTDZEnvironmentKey> : GenericHashTraits<JSC::CompactTDZEnvironmentKey> {
    static constexpr bool emptyValueIsZero = true;
    static JSC::CompactTDZEnvironmentKey emptyValue() { return JSC::CompactTDZEnvironmentKey(); }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(JSC::CompactTDZEnvironmentKey key) { return key.isHashTableEmptyValue(); }

    static void constructDeletedValue(JSC::CompactTDZEnvironmentKey& key) { JSC::CompactTDZEnvironmentKey::makeDeletedValue(key); }
    static bool isDeletedValue(JSC::CompactTDZEnvironmentKey key) { return key.isHashTableDeletedValue(); }
};

} // namespace WTF

namespace JSC {

class CompactTDZEnvironmentMap : public RefCounted<CompactTDZEnvironmentMap> {
public:
    class Handle {
        friend class CachedCompactTDZEnvironmentMapHandle;

    public:
        Handle() = default;

        Handle(CompactTDZEnvironment&, CompactTDZEnvironmentMap&);

        Handle(Handle&& other)
        {
            swap(other);
        }
        Handle& operator=(Handle&& other)
        {
            Handle handle(WTFMove(other));
            swap(handle);
            return *this;
        }

        Handle(const Handle&);
        Handle& operator=(const Handle& other)
        {
            Handle handle(other);
            swap(handle);
            return *this;
        }

        ~Handle();

        explicit operator bool() const { return !!m_map; }

        const CompactTDZEnvironment& environment() const
        {
            return *m_environment;
        }

    private:
        void swap(Handle& other)
        {
            std::swap(other.m_environment, m_environment);
            std::swap(other.m_map, m_map);
        }

        CompactTDZEnvironment* m_environment { nullptr };
        RefPtr<CompactTDZEnvironmentMap> m_map;
    };

    Handle get(const TDZEnvironment&);

private:
    friend class Handle;
    friend class CachedCompactTDZEnvironmentMapHandle;

    Handle get(CompactTDZEnvironment*, bool& isNewEntry);

    HashMap<CompactTDZEnvironmentKey, unsigned> m_map;
};

class TDZEnvironmentLink : public RefCounted<TDZEnvironmentLink> {
    TDZEnvironmentLink(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent)
        : m_handle(WTFMove(handle))
        , m_parent(WTFMove(parent))
    { }

public:
    static RefPtr<TDZEnvironmentLink> create(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent)
    {
        return adoptRef(new TDZEnvironmentLink(WTFMove(handle), WTFMove(parent)));
    }

    bool contains(UniquedStringImpl* impl) const { return m_handle.environment().toTDZEnvironment().contains(impl); }
    TDZEnvironmentLink* parent() { return m_parent.get(); }

private:
    friend class CachedTDZEnvironmentLink;

    CompactTDZEnvironmentMap::Handle m_handle;
    RefPtr<TDZEnvironmentLink> m_parent;
};

} // namespace JSC
