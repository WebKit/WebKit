/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "ClassInfo.h"
#include "JSCast.h"
#include "JSTypeInfo.h"
#include "PropertyOffset.h"
#include "PropertySlot.h"
#include <wtf/FixedVector.h>

namespace JSC {

class JSPropertyNameEnumerator;
class LLIntOffsetsExtractor;
class Structure;
class StructureChain;
class CachedSpecialPropertyAdaptiveStructureWatchpoint;
class CachedSpecialPropertyAdaptiveInferredPropertyValueWatchpoint;
struct SpecialPropertyCache;
enum class CachedPropertyNamesKind : uint8_t {
    Keys = 0,
    GetOwnPropertyNames,
};
static constexpr unsigned numberOfCachedPropertyNames = 2;

enum class CachedSpecialPropertyKey : uint8_t {
    ToStringTag = 0,
    ToString,
    ValueOf,
    ToPrimitive,
};
static constexpr unsigned numberOfCachedSpecialPropertyKeys = 4;

class StructureRareData;
class StructureChainInvalidationWatchpoint;

class StructureRareData final : public JSCell {
public:
    typedef JSCell Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.structureRareDataSpace;
    }

    static StructureRareData* create(VM&, Structure*);

    static constexpr bool needsDestruction = true;
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

    JSPropertyNameEnumerator* cachedPropertyNameEnumerator() const;
    uintptr_t cachedPropertyNameEnumeratorAndFlag() const;
    void setCachedPropertyNameEnumerator(VM&, Structure*, JSPropertyNameEnumerator*, StructureChain*);
    void clearCachedPropertyNameEnumerator();

    JSImmutableButterfly* cachedPropertyNames(CachedPropertyNamesKind) const;
    JSImmutableButterfly* cachedPropertyNamesIgnoringSentinel(CachedPropertyNamesKind) const;
    JSImmutableButterfly* cachedPropertyNamesConcurrently(CachedPropertyNamesKind) const;
    void setCachedPropertyNames(VM&, CachedPropertyNamesKind, JSImmutableButterfly*);

    Box<InlineWatchpointSet> copySharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }
    const Box<InlineWatchpointSet>& sharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }
    void setSharedPolyProtoWatchpoint(Box<InlineWatchpointSet>&& sharedPolyProtoWatchpoint) { m_polyProtoWatchpoint = WTFMove(sharedPolyProtoWatchpoint); }
    bool hasSharedPolyProtoWatchpoint() const { return static_cast<bool>(m_polyProtoWatchpoint); }

    static JSImmutableButterfly* cachedPropertyNamesSentinel() { return bitwise_cast<JSImmutableButterfly*>(static_cast<uintptr_t>(1)); }

    static ptrdiff_t offsetOfCachedPropertyNames(CachedPropertyNamesKind kind)
    {
        return OBJECT_OFFSETOF(StructureRareData, m_cachedPropertyNames) + sizeof(WriteBarrier<JSImmutableButterfly>) * static_cast<unsigned>(kind);
    }

    static ptrdiff_t offsetOfCachedPropertyNameEnumeratorAndFlag()
    {
        return OBJECT_OFFSETOF(StructureRareData, m_cachedPropertyNameEnumeratorAndFlag);
    }

    DECLARE_EXPORT_INFO;

    void finalizeUnconditionally(VM&);

    static constexpr uintptr_t cachedPropertyNameEnumeratorIsValidatedViaTraversingFlag = 1;
    static constexpr uintptr_t cachedPropertyNameEnumeratorMask = ~static_cast<uintptr_t>(1);

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

    WriteBarrier<Structure> m_previous;
    // FIXME: We should have some story for clearing these property names caches in GC.
    // https://bugs.webkit.org/show_bug.cgi?id=192659
    uintptr_t m_cachedPropertyNameEnumeratorAndFlag { 0 };
    FixedVector<StructureChainInvalidationWatchpoint> m_cachedPropertyNameEnumeratorWatchpoints;
    WriteBarrier<JSImmutableButterfly> m_cachedPropertyNames[numberOfCachedPropertyNames] { };

    typedef HashMap<PropertyOffset, RefPtr<WatchpointSet>, WTF::IntHash<PropertyOffset>, WTF::UnsignedWithZeroKeyHashTraits<PropertyOffset>> PropertyWatchpointMap;
#ifdef NDEBUG
    static_assert(sizeof(PropertyWatchpointMap) == sizeof(void*), "StructureRareData should remain small");
#endif

    PropertyWatchpointMap m_replacementWatchpointSets;
    std::unique_ptr<SpecialPropertyCache> m_specialPropertyCache;
    Box<InlineWatchpointSet> m_polyProtoWatchpoint;

    PropertyOffset m_maxOffset;
    PropertyOffset m_transitionOffset;
};

} // namespace JSC
