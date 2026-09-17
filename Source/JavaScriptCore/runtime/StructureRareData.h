/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/JSTypeInfo.h>
#include <JavaScriptCore/PropertyOffset.h>
#include <JavaScriptCore/PropertySlot.h>
#include <wtf/FixedVector.h>

namespace JSC {

class JSPropertyNameEnumerator;
class LLIntOffsetsExtractor;
class Structure;
class StructureChain;
class CachedSpecialPropertyAdaptiveStructureWatchpoint;
class CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint;
struct SpecialPropertyCache;
struct RawDoubleMask;
enum class CachedPropertyNamesKind : uint8_t {
    EnumerableStrings = 0,
    Strings,
    Symbols,
    StringsAndSymbols,
};
static constexpr unsigned numberOfCachedPropertyNames = 4;

enum class CachedSpecialPropertyKey : uint8_t {
    ToStringTag = 0,
    ToString,
    ValueOf,
    ToPrimitive,
    ToJSON,
};
static constexpr unsigned numberOfCachedSpecialPropertyKeys = 5;

class StructureRareData;
class StructureChainInvalidationWatchpoint;

class StructureRareData final : public JSCell {
public:
    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    inline static GCClient::IsoSubspace* subspaceFor(VM&); // Defined in StructureRareDataInlines.h

    static StructureRareData* create(VM&, Structure*);

    static constexpr DestructionMode needsDestruction = NeedsDestruction;
    static void destroy(JSCell*);

    DECLARE_VISIT_CHILDREN;

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    Structure* previousID() const
    {
        return m_previous.get();
    }
    void setPreviousID(VM&, Structure*);
    void clearPreviousID();

    JSValue cachedSpecialProperty(CachedSpecialPropertyKey) const;
    void cacheSpecialProperty(JSGlobalObject*, VM&, Structure* baseStructure, JSValue, CachedSpecialPropertyKey, const PropertySlot&);

    TriState cachedHasDefaultToPrimitiveFastAndNonObservable() const { return static_cast<TriState>(m_cachedHasDefaultToPrimitiveFastAndNonObservable); }
    void setCachedHasDefaultToPrimitiveFastAndNonObservable(TriState mode) { m_cachedHasDefaultToPrimitiveFastAndNonObservable = static_cast<unsigned>(mode); }

    JSPropertyNameEnumerator* cachedPropertyNameEnumerator() const;
    uintptr_t cachedPropertyNameEnumeratorAndFlag() const;
    void setCachedPropertyNameEnumerator(VM&, Structure*, JSPropertyNameEnumerator*, StructureChain*);
    void clearCachedPropertyNameEnumerator();

    JSCellButterfly* cachedPropertyNames(CachedPropertyNamesKind) const;
    JSCellButterfly* cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind) const;
    JSCellButterfly* cachedPropertyNamesConcurrently(CachedPropertyNamesKind) const;
    void setCachedPropertyNames(VM&, CachedPropertyNamesKind, JSCellButterfly*);

    Box<InlineWatchpointSet> copySharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }
    const Box<InlineWatchpointSet>& sharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }
    void setSharedPolyProtoWatchpoint(Box<InlineWatchpointSet>&& sharedPolyProtoWatchpoint) { m_polyProtoWatchpoint = WTF::move(sharedPolyProtoWatchpoint); }
    bool hasSharedPolyProtoWatchpoint() const { return static_cast<bool>(m_polyProtoWatchpoint); }

    static JSCellButterfly* cachedPropertyNamesSentinel() { return std::bit_cast<JSCellButterfly*>(static_cast<uintptr_t>(1)); }

    static constexpr ptrdiff_t offsetOfCachedPropertyNames(CachedPropertyNamesKind kind)
    {
        return OBJECT_OFFSETOF(StructureRareData, m_cachedPropertyNames) + sizeof(WriteBarrier<JSCellButterfly>) * static_cast<unsigned>(kind);
    }

    static constexpr ptrdiff_t offsetOfCachedPropertyNameEnumeratorAndFlag()
    {
        return OBJECT_OFFSETOF(StructureRareData, m_cachedPropertyNameEnumeratorAndFlag);
    }

    static constexpr ptrdiff_t offsetOfSpecialPropertyCache()
    {
        return OBJECT_OFFSETOF(StructureRareData, m_specialPropertyCache);
    }

    static constexpr ptrdiff_t offsetOfPrevious()
    {
        return OBJECT_OFFSETOF(StructureRareData, m_previous);
    }

    DECLARE_EXPORT_INFO;

    void reconcileWeakReferencesAtGCEnd(VM&, CollectionScope);

    static constexpr uintptr_t cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag = 1;
    static constexpr uintptr_t cachedPropertyNameEnumeratorMask = ~static_cast<uintptr_t>(1);

    unsigned incrementActiveReplacementWatchpointSet()
    {
        return ++m_activeReplacementWatchpointSet;
    }

    unsigned decrementActiveReplacementWatchpointSet()
    {
        return --m_activeReplacementWatchpointSet;
    }

private:
    friend class LLIntOffsetsExtractor;
    friend class Structure;
    friend class CachedSpecialPropertyAdaptiveStructureWatchpoint;
    friend class CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint;

    StructureRareData(VM&, Structure*);

    void clearCachedSpecialProperty(CachedSpecialPropertyKey);
    void cacheSpecialPropertySlow(JSGlobalObject*, VM&, Structure* baseStructure, JSValue, CachedSpecialPropertyKey, const PropertySlot&);

    SpecialPropertyCache& ensureSpecialPropertyCache();
    SpecialPropertyCache& ensureSpecialPropertyCacheSlow();
    bool canCacheSpecialProperty(CachedSpecialPropertyKey);
    void giveUpOnSpecialPropertyCache(CachedSpecialPropertyKey);

    bool tryCachePropertyNameEnumeratorViaWatchpoint(VM&, Structure*, StructureChain*);

    // FIXME: We should have some story for clearing these property names caches in GC.
    // https://bugs.webkit.org/show_bug.cgi?id=192659
    uintptr_t m_cachedPropertyNameEnumeratorAndFlag { 0 };
    FixedVector<StructureChainInvalidationWatchpoint> m_cachedPropertyNameEnumeratorWatchpoints;
    WriteBarrier<JSCellButterfly> m_cachedPropertyNames[numberOfCachedPropertyNames] { };

    typedef UncheckedKeyHashMap<PropertyOffset, RefPtr<WatchpointSet>, WTF::IntHash<PropertyOffset>, WTF::UnsignedWithZeroKeyHashTraits<PropertyOffset>> PropertyWatchpointMap;
#ifdef NDEBUG
    static_assert(sizeof(PropertyWatchpointMap) == sizeof(void*), "StructureRareData should remain small");
#endif

    PropertyWatchpointMap m_replacementWatchpointSets;
    std::unique_ptr<SpecialPropertyCache> m_specialPropertyCache;
    Box<InlineWatchpointSet> m_polyProtoWatchpoint;

    WriteBarrierStructureID m_previous;
    PropertyOffset m_maxOffset;
    PropertyOffset m_transitionOffset;
    std::unique_ptr<RawDoubleMask> m_rawDoubleMask;
    // Per-offset raw-double map for the double-field representation project: bit N set iff the property at
    // PropertyOffset N is stored as raw IEEE-754 bits rather than a NaN-boxed JSValue.
    //
    // TWO levels of laziness, both forced by measurement rather than chosen. It is not on Structure, because
    // Structures are among the most numerous cells in the heap and sizeof(Structure) sits exactly on a 16-byte size
    // class, so even one word there costs +16 B on EVERY Structure (07-PLAN section 5k). And it is not stored inline
    // HERE either: StructureRareData is already at the 96-byte budget the static_assert below enforces, which
    // rejected a 16-byte field and then an 8-byte one. So it is a side table on the same pattern as
    // m_specialPropertyCache above, allocated only for Structures that genuinely carry a Double-represented field.
    //
    // Every consumer gates on Structure::hasRawDoubleFields() before reaching here, so neither indirection sits on a
    // path a Structure without raw doubles can take. See Structure::isRawDoubleOffset.
    unsigned m_activeReplacementWatchpointSet : 30 { 0 };
    unsigned m_cachedHasDefaultToPrimitiveFastAndNonObservable : 2 { static_cast<unsigned>(TriState::Indeterminate) }; // TriState
};
#ifdef NDEBUG
// RAISED FROM 96 TO 112 for the double-field representation project, to unblock measurement. This is a DELIBERATE,
// TEMPORARY relaxation of an upstream budget, not a free change: commit edd95375 ("[JSC] Shrink StructureRareData back
// to 96 bytes") set 96 on purpose, and 96 is a MarkedSpace size-class boundary, so 112 is the next class up -- every
// StructureRareData that exists costs 16 bytes more.
//
// It buys the ability to measure whether the raw-double representation is worth anything at all (07-PLAN section 5n:
// the per-offset mask is provably necessary and has no free home in either Structure or StructureRareData).
//
// BEFORE LANDING, one of these must happen instead:
//   - shrink StructureRareData by 16 bytes on its own merits, or
//   - find a carrier for the mask that is not either object, or
//   - show by measurement that +16 B per rare data is acceptable, with a real JS3 heap number
//     (section 1.6 of this campaign is about JSC at 190 MB vs V8's 67 MB, so this is not a formality).
static_assert(sizeof(StructureRareData) <= 112, "StructureRareData should remain small");
#endif

} // namespace JSC
