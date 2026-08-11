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

#include <JavaScriptCore/Identifier.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/InlineMap.h>
#include <wtf/IteratorRange.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/TZoneMalloc.h>

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
    ALWAYS_INLINE bool isFunctionDeclaration() const { return m_bits & IsFunctionDeclaration; }
    ALWAYS_INLINE bool isParameter() const { return m_bits & IsParameter; }
    ALWAYS_INLINE bool isSloppyModeHoistedFunction() const { return m_bits & IsSloppyModeHoistedFunction; }
    ALWAYS_INLINE bool isPrivateField() const { return m_bits & IsPrivateField; }
    ALWAYS_INLINE bool isPrivateMethod() const { return m_bits & IsPrivateMethod; }
    ALWAYS_INLINE bool isPrivateSetter() const { return m_bits & IsPrivateSetter; }
    ALWAYS_INLINE bool isPrivateGetter() const { return m_bits & IsPrivateGetter; }
    ALWAYS_INLINE bool isUsing() const { return m_bits & IsUsing; }

    ALWAYS_INLINE void setIsCaptured() { m_bits |= IsCaptured; }
    ALWAYS_INLINE void setIsConst() { m_bits |= IsConst; }
    ALWAYS_INLINE void setIsVar() { m_bits |= IsVar; }
    ALWAYS_INLINE void setIsLet() { m_bits |= IsLet; }
    ALWAYS_INLINE void setIsExported() { m_bits |= IsExported; }
    ALWAYS_INLINE void setIsImported() { m_bits |= IsImported; }
    ALWAYS_INLINE void setIsImportedNamespace() { m_bits |= IsImportedNamespace; }
    ALWAYS_INLINE void setIsFunction() { m_bits |= IsFunction; }
    ALWAYS_INLINE void setIsFunctionDeclaration() { m_bits |= IsFunctionDeclaration; }
    ALWAYS_INLINE void setIsParameter() { m_bits |= IsParameter; }
    ALWAYS_INLINE void setIsSloppyModeHoistedFunction() { m_bits |= IsSloppyModeHoistedFunction; }
    ALWAYS_INLINE void setIsPrivateField() { m_bits |= IsPrivateField; }
    ALWAYS_INLINE void setIsPrivateMethod() { m_bits |= IsPrivateMethod; }
    ALWAYS_INLINE void setIsPrivateSetter() { m_bits |= IsPrivateSetter; }
    ALWAYS_INLINE void setIsPrivateGetter() { m_bits |= IsPrivateGetter; }
    ALWAYS_INLINE void setIsUsing() { m_bits |= IsUsing; }

    ALWAYS_INLINE void clearIsVar() { m_bits &= ~IsVar; }

    uint16_t bits() const { return m_bits; }

    friend bool operator==(const VariableEnvironmentEntry&, const VariableEnvironmentEntry&) = default;

    void dump(PrintStream&) const;

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
        IsSloppyModeHoistedFunction = 1 << 9,
        IsPrivateField = 1 << 10,
        IsPrivateMethod = 1 << 11,
        IsPrivateGetter = 1 << 12,
        IsPrivateSetter = 1 << 13,
        IsFunctionDeclaration = 1 << 14,
        IsUsing = 1 << 15,
    };
    uint16_t m_bits { 0 };
};

struct VariableEnvironmentEntryHashTraits : HashTraits<VariableEnvironmentEntry> {
    static constexpr bool needsDestruction = false;
};

struct PrivateNameEntry {
    friend class CachedPrivateNameEntry;

    static constexpr unsigned privateClassBrandOffset = 0;
    static constexpr unsigned privateBrandOffset = 1;

public:
    PrivateNameEntry(uint16_t traits = 0) { m_bits = traits; }

    ALWAYS_INLINE bool isMethod() const { return m_bits & IsMethod; }
    ALWAYS_INLINE bool isSetter() const { return m_bits & IsSetter; }
    ALWAYS_INLINE bool isGetter() const { return m_bits & IsGetter; }
    ALWAYS_INLINE bool isField() const { return !isPrivateMethodOrAccessor(); }
    ALWAYS_INLINE bool isStatic() const { return m_bits & IsStatic; }

    bool isPrivateMethodOrAccessor() const { return isMethod() || isSetter() || isGetter(); }

    uint16_t bits() const { return m_bits; }

    friend bool operator==(const PrivateNameEntry&, const PrivateNameEntry&) = default;

    enum Traits : uint16_t {
        None = 0,
        IsMethod = 1 << 0,
        IsGetter = 1 << 1,
        IsSetter = 1 << 2,
        IsStatic = 1 << 3,
    };

private:
    uint16_t m_bits { 0 };
};

struct PrivateNameEntryHashTraits : HashTraits<PrivateNameEntry> {
    static constexpr bool needsDestruction = false;
};

typedef UncheckedKeyHashMap<PackedRefPtr<UniquedStringImpl>, PrivateNameEntry, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, PrivateNameEntryHashTraits> PrivateNameEnvironment;

class VariableEnvironment {
    WTF_MAKE_TZONE_ALLOCATED(VariableEnvironment);

public:
    static constexpr unsigned inlineMapCapacity = 9;

private:
    typedef InlineMap<PackedRefPtr<UniquedStringImpl>, VariableEnvironmentEntry, inlineMapCapacity, IdentifierRepHash, HashTraits<RefPtr<UniquedStringImpl>>, VariableEnvironmentEntryHashTraits> Map;

public:

    VariableEnvironment() { }
    VariableEnvironment(VariableEnvironment&& other)
        : m_map(WTF::move(other.m_map))
        , m_isEverythingCaptured(other.m_isEverythingCaptured)
        , m_hasAwaitUsingDeclaration(other.m_hasAwaitUsingDeclaration)
        , m_rareData(WTF::move(other.m_rareData))
    {
    }
    // Defined in VariableEnvironmentInlines.h.
    VariableEnvironment(const VariableEnvironment& other);
    VariableEnvironment& operator=(const VariableEnvironment& other);

    ALWAYS_INLINE Map::iterator begin() { return m_map.begin(); }
    ALWAYS_INLINE Map::iterator end() { return m_map.end(); }
    ALWAYS_INLINE Map::const_iterator begin() const { return m_map.begin(); }
    ALWAYS_INLINE Map::const_iterator end() const { return m_map.end(); }
    ALWAYS_INLINE Map::AddResult add(const RefPtr<UniquedStringImpl>& identifier) { return m_map.add(identifier, VariableEnvironmentEntry()); }
    ALWAYS_INLINE Map::AddResult add(const Identifier& identifier) { return add(identifier.impl()); }

    // Defined in VariableEnvironmentInlines.h.
    PrivateNameEnvironment::AddResult addPrivateName(const Identifier& identifier);
    // Defined in VariableEnvironmentInlines.h.
    PrivateNameEnvironment::AddResult addPrivateName(const RefPtr<UniquedStringImpl>& identifier);

    ALWAYS_INLINE unsigned size() const { return m_map.size() + privateNamesSize(); }
    ALWAYS_INLINE unsigned mapSize() const { return m_map.size(); }
    ALWAYS_INLINE bool contains(const UniquedStringImpl* identifier) const { return m_map.contains(identifier); }
    ALWAYS_INLINE bool remove(const UniquedStringImpl* identifier) { return m_map.remove(identifier); }
    ALWAYS_INLINE Map::iterator find(const UniquedStringImpl* identifier) { return m_map.find(identifier); }
    ALWAYS_INLINE Map::const_iterator find(const UniquedStringImpl* identifier) const { return m_map.find(identifier); }
    void swap(VariableEnvironment& other);
    void markVariableAsCapturedIfDefined(const UniquedStringImpl* identifier);
    void markVariableAsCaptured(const UniquedStringImpl* identifier);
    void markAllVariablesAsCaptured();
    bool hasCapturedVariables() const;
    bool captures(UniquedStringImpl* identifier) const;
    void markVariableAsImported(const UniquedStringImpl* identifier);
    void markVariableAsExported(const UniquedStringImpl* identifier);

    bool isEverythingCaptured() const { return m_isEverythingCaptured; }
    bool hasUsingDeclaration() const
    {
        for (auto& pair : m_map) {
            if (pair.value.isUsing())
                return true;
        }
        return false;
    }
    unsigned usingDeclarationCount() const
    {
        unsigned count = 0;
        for (auto& pair : m_map) {
            if (pair.value.isUsing())
                count++;
        }
        return count;
    }
    bool hasAwaitUsingDeclaration() const { return m_hasAwaitUsingDeclaration; }
    void setHasAwaitUsingDeclaration() { m_hasAwaitUsingDeclaration = true; }
    bool isEmpty() const { return !m_map.size() && !privateNamesSize(); }

    using PrivateNamesRange = WTF::IteratorRange<PrivateNameEnvironment::iterator>;

    // Defined in VariableEnvironmentInlines.h.
    Map::AddResult declarePrivateField(const Identifier& identifier);

    // Defined in VariableEnvironmentInlines.h.
    bool declarePrivateMethod(const Identifier& identifier);
    bool declarePrivateMethod(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits addionalTraits = PrivateNameEntry::Traits::None);

    enum class PrivateDeclarationResult {
        Success,
        DuplicatedName,
        InvalidStaticNonStatic
    };

    PrivateDeclarationResult declarePrivateAccessor(const RefPtr<UniquedStringImpl>&, PrivateNameEntry accessorTraits);

    // Defined in VariableEnvironmentInlines.h.
    bool declareStaticPrivateMethod(const Identifier& identifier);

    // Defined in VariableEnvironmentInlines.h.
    PrivateDeclarationResult declarePrivateSetter(const Identifier& identifier);
    // Defined in VariableEnvironmentInlines.h.
    PrivateDeclarationResult declareStaticPrivateSetter(const Identifier& identifier);
    PrivateDeclarationResult declarePrivateSetter(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits modifierTraits = PrivateNameEntry::Traits::None);

    // Defined in VariableEnvironmentInlines.h.
    PrivateDeclarationResult declarePrivateGetter(const Identifier& identifier);
    // Defined in VariableEnvironmentInlines.h.
    PrivateDeclarationResult declareStaticPrivateGetter(const Identifier& identifier);
    PrivateDeclarationResult declarePrivateGetter(const RefPtr<UniquedStringImpl>& identifier, PrivateNameEntry::Traits modifierTraits = PrivateNameEntry::Traits::None);

    Map::AddResult declarePrivateField(const RefPtr<UniquedStringImpl>&);

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

    ALWAYS_INLINE PrivateNameEnvironment* privateNameEnvironment()
    {
        if (!m_rareData)
            return nullptr;
        return &m_rareData->m_privateNames;
    }

    ALWAYS_INLINE const PrivateNameEnvironment* privateNameEnvironment() const
    {
        if (!m_rareData)
            return nullptr;
        return &m_rareData->m_privateNames;
    }

    ALWAYS_INLINE bool hasStaticPrivateMethodOrAccessor() const
    {
        if (!m_rareData)
            return false;

        for (auto entry : privateNames()) {
            if (entry.value.isPrivateMethodOrAccessor() && entry.value.isStatic())
                return true;
        }
        
        return false;
    }
    
    ALWAYS_INLINE bool hasInstancePrivateMethodOrAccessor() const
    {
        if (!m_rareData)
            return false;
        
        for (auto entry : privateNames()) {
            if (entry.value.isPrivateMethodOrAccessor() && !entry.value.isStatic())
                return true;
        }
        
        return false;
    }

    ALWAYS_INLINE bool hasPrivateName(const Identifier& identifier)
    {
        if (!m_rareData)
            return false;
        return m_rareData->m_privateNames.contains(identifier.impl());
    }

    // Defined in VariableEnvironmentInlines.h.
    void addPrivateNamesFrom(const PrivateNameEnvironment* privateNameEnvironment);

    struct RareData {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(RareData);

        RareData() { }
        RareData(RareData&& other)
            : m_privateNames(WTF::move(other.m_privateNames))
        {
        }
        RareData(const RareData&) = default;
        RareData& operator=(const RareData&) = default;
        PrivateNameEnvironment m_privateNames;
    };

    void dump(PrintStream&) const;

private:
    friend class CachedVariableEnvironment;

    Map m_map;
    bool m_isEverythingCaptured { false };
    bool m_hasAwaitUsingDeclaration { false };

    // Defined in VariableEnvironmentInlines.h.
    PrivateNameEntry& getOrAddPrivateName(UniquedStringImpl* impl);

    std::unique_ptr<VariableEnvironment::RareData> m_rareData;
};

using TDZEnvironment = UncheckedKeyHashSet<RefPtr<UniquedStringImpl>, IdentifierRepHash>;

class CompactTDZEnvironment {
    WTF_MAKE_TZONE_ALLOCATED(CompactTDZEnvironment);
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

    // Defined in VariableEnvironmentInlines.h.
    TDZEnvironment& toTDZEnvironment() const;

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
    // Defined in VariableEnvironmentInlines.h.
    static bool equal(const CompactTDZEnvironmentKey& a, const CompactTDZEnvironmentKey& b);
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
        // Defined in VariableEnvironmentInlines.h.
        Handle& operator=(Handle&& other);

        Handle(const Handle&);
        // Defined in VariableEnvironmentInlines.h.
        Handle& operator=(const Handle& other);

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

    UncheckedKeyHashMap<CompactTDZEnvironmentKey, unsigned> m_map;
};

class TDZEnvironmentLink : public RefCounted<TDZEnvironmentLink> {
    // Defined in VariableEnvironmentInlines.h.
    TDZEnvironmentLink(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent);

public:
    // Defined in VariableEnvironmentInlines.h.
    static RefPtr<TDZEnvironmentLink> create(CompactTDZEnvironmentMap::Handle handle, RefPtr<TDZEnvironmentLink> parent);

    // Defined in VariableEnvironmentInlines.h.
    bool contains(UniquedStringImpl* impl) const;
    TDZEnvironmentLink* parent() { return m_parent.get(); }

private:
    friend class CachedTDZEnvironmentLink;

    CompactTDZEnvironmentMap::Handle m_handle;
    RefPtr<TDZEnvironmentLink> m_parent;
};

} // namespace JSC
