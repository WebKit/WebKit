/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/PropertyDescriptor.h>
#include <JavaScriptCore/PutDirectIndexMode.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

class SparseArrayValueMap;

class SparseArrayEntry {
    WTF_MAKE_TZONE_ALLOCATED(SparseArrayEntry);
public:
    static constexpr unsigned isEmptyBucketFlag = 1u << 31;
    static constexpr unsigned isDeletedBucketFlag = 1u << 30;
    static_assert(PropertyAttribute::LastAttribute < isDeletedBucketFlag);

    enum EmptyBucketTag { EmptyBucket };
    enum DeletedBucketTag { DeletedBucket };

    SparseArrayEntry() = default;

    explicit SparseArrayEntry(unsigned index)
        : m_index(index)
    {
    }

    explicit SparseArrayEntry(EmptyBucketTag)
        : m_attributes(isEmptyBucketFlag)
    {
    }

    explicit SparseArrayEntry(DeletedBucketTag)
        : m_attributes(isDeletedBucketFlag)
    {
    }

    void NODELETE get(JSObject*, PropertySlot&) const;
    void get(PropertyDescriptor&) const;
    bool put(JSGlobalObject*, JSValue thisValue, SparseArrayValueMap*, JSValue, bool shouldThrow);
    JSValue NODELETE getNonSparseMode() const;
    JSValue getConcurrently() const;
    JSValue get() const;

    unsigned index() const { return m_index; }
    unsigned attributes() const { return m_attributes; }

    bool isEmptyBucket() const { return m_attributes & isEmptyBucketFlag; }
    bool isDeletedBucket() const { return m_attributes & isDeletedBucketFlag; }

    void forceSet(SparseArrayValueMap*, unsigned attributes);
    void forceSet(VM&, SparseArrayValueMap*, JSValue, unsigned attributes);

    WriteBarrier<Unknown>& asValue() { return m_value; }
    const WriteBarrier<Unknown>& asValue() const { return m_value; }

private:
    unsigned m_attributes { 0 };
    unsigned m_index { 0 };
    WriteBarrier<Unknown> m_value { UndefinedWriteBarrierTag };
};

struct SparseArrayEntryHash {
    static unsigned hash(const SparseArrayEntry& entry) { return WTF::IntHash<uint32_t>::hash(entry.index()); }
    static bool equal(const SparseArrayEntry& a, const SparseArrayEntry& b) { return a.index() == b.index(); }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct SparseArrayEntryHashTraits : WTF::GenericHashTraits<SparseArrayEntry> {
    static constexpr bool emptyValueIsZero = false;
    static constexpr bool hasIsEmptyValueFunction = true;
    static SparseArrayEntry emptyValue() { return SparseArrayEntry(SparseArrayEntry::EmptyBucket); }
    static bool isEmptyValue(const SparseArrayEntry& entry) { return entry.isEmptyBucket(); }
    static void constructDeletedValue(SparseArrayEntry& slot) { new (NotNull, std::addressof(slot)) SparseArrayEntry(SparseArrayEntry::DeletedBucket); }
    static bool isDeletedValue(const SparseArrayEntry& entry) { return entry.isDeletedBucket(); }
};

struct SparseArrayEntryTranslator {
    static unsigned hash(uint32_t key) { return WTF::IntHash<uint32_t>::hash(key); }
    static bool equal(const SparseArrayEntry& entry, uint32_t key) { return entry.index() == key; }
};

class SparseArrayValueMap final : public JSCell {
public:
    using Base = JSCell;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

private:
    using Set = UncheckedKeyHashSet<SparseArrayEntry, SparseArrayEntryHash, SparseArrayEntryHashTraits>;

    enum Flags {
        Normal                             = 0,
        SparseMode                         = 1 << 0,
        LengthIsReadOnly                   = 1 << 1,
        HasAnyKindOfGetterSetterProperties = 1 << 2,
    };

    SparseArrayValueMap(VM&);

    DECLARE_DEFAULT_FINISH_CREATION;

public:
    DECLARE_EXPORT_INFO;

    using iterator = Set::iterator;
    using const_iterator = Set::const_iterator;
    using AddResult = Set::AddResult;

    static SparseArrayValueMap* create(VM&);

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    template<typename CellType, SubspaceAccess>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.sparseArrayValueMapSpace();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    DECLARE_VISIT_CHILDREN;

    bool sparseMode()
    {
        return m_flags & SparseMode;
    }

    void setSparseMode()
    {
        m_flags = static_cast<Flags>(m_flags | SparseMode);
    }

    bool lengthIsReadOnly()
    {
        return m_flags & LengthIsReadOnly;
    }

    void setLengthIsReadOnly()
    {
        m_flags = static_cast<Flags>(m_flags | LengthIsReadOnly);
    }

    bool hasAnyKindOfGetterSetterProperties()
    {
        return m_flags & HasAnyKindOfGetterSetterProperties;
    }

    void setHasAnyKindOfGetterSetterProperties()
    {
        m_flags = static_cast<Flags>(m_flags | HasAnyKindOfGetterSetterProperties);
    }

    static SparseArrayEntry& entryFor(const_iterator it) { return const_cast<SparseArrayEntry&>(*it); }

    // These methods may mutate the contents of the map
    bool putEntry(JSGlobalObject*, JSObject*, unsigned, JSValue, bool shouldThrow);
    bool putDirect(JSGlobalObject*, JSObject*, unsigned, JSValue, unsigned attributes, PutDirectIndexMode);
    AddResult add(JSObject*, unsigned);
    iterator find(unsigned i) { return m_set.find<SparseArrayEntryTranslator>(i); }
    // This should ASSERT the remove is valid (check the result of the find).
    void remove(iterator);
    void remove(unsigned i);

    JSValue getConcurrently(unsigned index);

    // These methods do not mutate the contents of the map.
    iterator notFound() { return m_set.end(); }
    bool isEmpty() const { return m_set.isEmpty(); }
    bool contains(unsigned i) const { return m_set.contains<SparseArrayEntryTranslator>(i); }
    size_t size() const { return m_set.size(); }
    // Only allow const begin/end iteration.
    const_iterator begin() const { return m_set.begin(); }
    const_iterator end() const { return m_set.end(); }

private:
    Set m_set;
    Flags m_flags { Normal };
    size_t m_reportedCapacity { 0 };
};

} // namespace JSC
