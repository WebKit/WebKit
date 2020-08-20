/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

namespace JSC {

class JSPropertyNameEnumerator;
class Structure;
class ObjectToStringAdaptiveInferredPropertyValueWatchpoint;
class ObjectToStringAdaptiveStructureWatchpoint;
enum class CachedPropertyNamesKind : uint8_t {
    Keys = 0,
    GetOwnPropertyNames,
};
static constexpr unsigned numberOfCachedPropertyNames = 2;

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

    static void visitChildren(JSCell*, SlotVisitor&);

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    Structure* previousID() const
    {
        return m_previous.get();
    }
    void setPreviousID(VM&, Structure*);
    void clearPreviousID();

    JSString* objectToStringValue() const;
    void setObjectToStringValue(JSGlobalObject*, VM&, Structure* baseStructure, JSString* value, const PropertySlot& toStringTagSymbolSlot);
    void giveUpOnObjectToStringValueCache() { m_objectToStringValue.setWithoutWriteBarrier(objectToStringCacheGiveUpMarker()); }
    bool canCacheObjectToStringValue() { return m_objectToStringValue.unvalidatedGet() == objectToStringCacheGiveUpMarker(); }
    static JSString* objectToStringCacheGiveUpMarker() { return bitwise_cast<JSString*>(static_cast<uintptr_t>(1)); }

    JSPropertyNameEnumerator* cachedPropertyNameEnumerator() const;
    void setCachedPropertyNameEnumerator(VM&, JSPropertyNameEnumerator*);

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

    DECLARE_EXPORT_INFO;

    void finalizeUnconditionally(VM&);

private:
    friend class Structure;
    friend class ObjectToStringAdaptiveStructureWatchpoint;
    friend class ObjectToStringAdaptiveInferredPropertyValueWatchpoint;

    void clearObjectToStringValue();

    StructureRareData(VM&, Structure*);

    WriteBarrier<Structure> m_previous;
    WriteBarrier<JSString> m_objectToStringValue;
    // FIXME: We should have some story for clearing these property names caches in GC.
    // https://bugs.webkit.org/show_bug.cgi?id=192659
    WriteBarrier<JSPropertyNameEnumerator> m_cachedPropertyNameEnumerator;
    WriteBarrier<JSImmutableButterfly> m_cachedPropertyNames[numberOfCachedPropertyNames] { };

    typedef HashMap<PropertyOffset, RefPtr<WatchpointSet>, WTF::IntHash<PropertyOffset>, WTF::UnsignedWithZeroKeyHashTraits<PropertyOffset>> PropertyWatchpointMap;
    std::unique_ptr<PropertyWatchpointMap> m_replacementWatchpointSets;
    Bag<ObjectToStringAdaptiveStructureWatchpoint> m_objectToStringAdaptiveWatchpointSet;
    std::unique_ptr<ObjectToStringAdaptiveInferredPropertyValueWatchpoint> m_objectToStringAdaptiveInferredValueWatchpoint;
    Box<InlineWatchpointSet> m_polyProtoWatchpoint;

    PropertyOffset m_maxOffset;
    PropertyOffset m_transitionOffset;
};

} // namespace JSC
