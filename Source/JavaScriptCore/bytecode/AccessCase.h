/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include "CacheableIdentifier.h"
#include "GCAwareJITStubRoutine.h"
#include "JITStubRoutine.h"
#include "JSFunctionInlines.h"
#include "ObjectPropertyConditionSet.h"
#include "PolyProtoAccessChain.h"
#include <wtf/CommaPrinter.h>
#include <wtf/VectorHash.h>

namespace JSC {

class GetterSetterAccessCase;
class InstanceOfAccessCase;
class IntrinsicGetterAccessCase;
class ModuleNamespaceAccessCase;
class ProxyableAccessCase;

class InlineCacheCompiler;
class InlineCacheHandler;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AccessCase);

// An AccessCase describes one of the cases of a PolymorphicAccess. A PolymorphicAccess represents a
// planned (to generate in future) or generated stub for some inline cache. That stub contains fast
// path code for some finite number of fast cases, each described by an AccessCase object.
//
// An AccessCase object has a lifecycle that proceeds through several states. Note that the states
// of AccessCase have a lot to do with the global effect epoch (we'll say epoch for short). This is
// a simple way of reasoning about the state of the system outside this AccessCase. Any observable
// effect - like storing to a property, changing an object's structure, etc. - increments the epoch.
// The states are:
//
// Primordial:   This is an AccessCase that was just allocated. It does not correspond to any actual
//               code and it is not owned by any PolymorphicAccess. In this state, the AccessCase
//               assumes that it is in the same epoch as when it was created. This is important
//               because it may make claims about itself ("I represent a valid case so long as you
//               register a watchpoint on this set") that could be contradicted by some outside
//               effects (like firing and deleting the watchpoint set in question). This is also the
//               state that an AccessCase is in when it is cloned (AccessCase::clone()).
//
// Committed:    This happens as soon as some PolymorphicAccess takes ownership of this AccessCase.
//               In this state, the AccessCase no longer assumes anything about the epoch. To
//               accomplish this, PolymorphicAccess calls AccessCase::commit(). This must be done
//               during the same epoch when the AccessCase was created, either by the client or by
//               clone(). When created by the client, committing during the same epoch works because
//               we can be sure that whatever watchpoint sets they spoke of are still valid. When
//               created by clone(), we can be sure that the set is still valid because the original
//               of the clone still has watchpoints on it.
//
// Generated:    This is the state when the PolymorphicAccess generates code for this case by
//               calling AccessCase::generate() or AccessCase::generateWithGuard(). At this point
//               the case object will have some extra stuff in it, like possibly the CallLinkInfo
//               object associated with the inline cache.
//               FIXME: Moving into the Generated state should not mutate the AccessCase object or
//               put more stuff into it. If we fix this, then we can get rid of AccessCase::clone().
//               https://bugs.webkit.org/show_bug.cgi?id=156456
//
// An AccessCase may be destroyed while in any of these states.
//
// We will sometimes buffer committed AccessCases in the PolymorphicAccess object before generating
// code. This allows us to only regenerate once we've accumulated (hopefully) more than one new
// AccessCase.

#define JSC_FOR_EACH_ACCESS_TYPE(macro) \
    macro(Load) \
    macro(LoadMegamorphic) \
    macro(Transition) \
    macro(StoreMegamorphic) \
    macro(Delete) \
    macro(DeleteNonConfigurable) \
    macro(DeleteMiss) \
    macro(Replace) \
    macro(Miss) \
    macro(GetGetter) \
    macro(Getter) \
    macro(Setter) \
    macro(CustomValueGetter) \
    macro(CustomAccessorGetter) \
    macro(CustomValueSetter) \
    macro(CustomAccessorSetter) \
    macro(IntrinsicGetter) \
    macro(InHit) \
    macro(InMiss) \
    macro(ArrayLength) \
    macro(StringLength) \
    macro(DirectArgumentsLength) \
    macro(ScopedArgumentsLength) \
    macro(ModuleNamespaceLoad) \
    macro(ProxyObjectHas) \
    macro(ProxyObjectLoad) \
    macro(ProxyObjectStore) \
    macro(InstanceOfHit) \
    macro(InstanceOfMiss) \
    macro(InstanceOfGeneric) \
    macro(CheckPrivateBrand) \
    macro(SetPrivateBrand) \
    macro(IndexedProxyObjectLoad) \
    macro(IndexedMegamorphicLoad) \
    macro(IndexedInt32Load) \
    macro(IndexedDoubleLoad) \
    macro(IndexedContiguousLoad) \
    macro(IndexedArrayStorageLoad) \
    macro(IndexedScopedArgumentsLoad) \
    macro(IndexedDirectArgumentsLoad) \
    macro(IndexedTypedArrayInt8Load) \
    macro(IndexedTypedArrayUint8Load) \
    macro(IndexedTypedArrayUint8ClampedLoad) \
    macro(IndexedTypedArrayInt16Load) \
    macro(IndexedTypedArrayUint16Load) \
    macro(IndexedTypedArrayInt32Load) \
    macro(IndexedTypedArrayUint32Load) \
    macro(IndexedTypedArrayFloat32Load) \
    macro(IndexedTypedArrayFloat64Load) \
    macro(IndexedResizableTypedArrayInt8Load) \
    macro(IndexedResizableTypedArrayUint8Load) \
    macro(IndexedResizableTypedArrayUint8ClampedLoad) \
    macro(IndexedResizableTypedArrayInt16Load) \
    macro(IndexedResizableTypedArrayUint16Load) \
    macro(IndexedResizableTypedArrayInt32Load) \
    macro(IndexedResizableTypedArrayUint32Load) \
    macro(IndexedResizableTypedArrayFloat32Load) \
    macro(IndexedResizableTypedArrayFloat64Load) \
    macro(IndexedStringLoad) \
    macro(IndexedNoIndexingMiss) \
    macro(IndexedMegamorphicStore) \
    macro(IndexedInt32Store) \
    macro(IndexedDoubleStore) \
    macro(IndexedContiguousStore) \
    macro(IndexedArrayStorageStore) \
    macro(IndexedTypedArrayInt8Store) \
    macro(IndexedTypedArrayUint8Store) \
    macro(IndexedTypedArrayUint8ClampedStore) \
    macro(IndexedTypedArrayInt16Store) \
    macro(IndexedTypedArrayUint16Store) \
    macro(IndexedTypedArrayInt32Store) \
    macro(IndexedTypedArrayUint32Store) \
    macro(IndexedTypedArrayFloat32Store) \
    macro(IndexedTypedArrayFloat64Store) \
    macro(IndexedResizableTypedArrayInt8Store) \
    macro(IndexedResizableTypedArrayUint8Store) \
    macro(IndexedResizableTypedArrayUint8ClampedStore) \
    macro(IndexedResizableTypedArrayInt16Store) \
    macro(IndexedResizableTypedArrayUint16Store) \
    macro(IndexedResizableTypedArrayInt32Store) \
    macro(IndexedResizableTypedArrayUint32Store) \
    macro(IndexedResizableTypedArrayFloat32Store) \
    macro(IndexedResizableTypedArrayFloat64Store) \
    macro(IndexedInt32InHit) \
    macro(IndexedDoubleInHit) \
    macro(IndexedContiguousInHit) \
    macro(IndexedArrayStorageInHit) \
    macro(IndexedScopedArgumentsInHit) \
    macro(IndexedDirectArgumentsInHit) \
    macro(IndexedTypedArrayInt8InHit) \
    macro(IndexedTypedArrayUint8InHit) \
    macro(IndexedTypedArrayUint8ClampedInHit) \
    macro(IndexedTypedArrayInt16InHit) \
    macro(IndexedTypedArrayUint16InHit) \
    macro(IndexedTypedArrayInt32InHit) \
    macro(IndexedTypedArrayUint32InHit) \
    macro(IndexedTypedArrayFloat32InHit) \
    macro(IndexedTypedArrayFloat64InHit) \
    macro(IndexedResizableTypedArrayInt8InHit) \
    macro(IndexedResizableTypedArrayUint8InHit) \
    macro(IndexedResizableTypedArrayUint8ClampedInHit) \
    macro(IndexedResizableTypedArrayInt16InHit) \
    macro(IndexedResizableTypedArrayUint16InHit) \
    macro(IndexedResizableTypedArrayInt32InHit) \
    macro(IndexedResizableTypedArrayUint32InHit) \
    macro(IndexedResizableTypedArrayFloat32InHit) \
    macro(IndexedResizableTypedArrayFloat64InHit) \
    macro(IndexedStringInHit) \
    macro(IndexedNoIndexingInMiss) \


class AccessCase : public ThreadSafeRefCounted<AccessCase> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AccessCase);
public:
    friend class InlineCacheCompiler;
    enum AccessType : uint8_t {
#define JSC_DEFINE_ACCESS_TYPE(name) name,
        JSC_FOR_EACH_ACCESS_TYPE(JSC_DEFINE_ACCESS_TYPE)
#undef JSC_DEFINE_ACCESS_TYPE
    };

    enum State : uint8_t {
        Primordial,
        Committed,
        Generated
    };

    template<typename T>
    T& as() { return *static_cast<T*>(this); }

    template<typename T>
    const T& as() const { return *static_cast<const T*>(this); }


    static Ref<AccessCase> create(VM&, JSCell* owner, AccessType, CacheableIdentifier, PropertyOffset = invalidOffset,
        Structure* = nullptr, const ObjectPropertyConditionSet& = ObjectPropertyConditionSet(), RefPtr<PolyProtoAccessChain>&& = nullptr);

    static RefPtr<AccessCase> createTransition(VM&, JSCell* owner, CacheableIdentifier, PropertyOffset, Structure* oldStructure,
        Structure* newStructure, const ObjectPropertyConditionSet&, RefPtr<PolyProtoAccessChain>&&, const StructureStubInfo&);

    static Ref<AccessCase> createDelete(VM&, JSCell* owner, CacheableIdentifier, PropertyOffset, Structure* oldStructure, Structure* newStructure);

    static Ref<AccessCase> createCheckPrivateBrand(VM&, JSCell* owner, CacheableIdentifier, Structure*);

    static Ref<AccessCase> createSetPrivateBrand(VM&, JSCell* owner, CacheableIdentifier, Structure* oldStructure, Structure* newStructure);

    static Ref<AccessCase> createReplace(VM&, JSCell* owner, CacheableIdentifier, PropertyOffset, Structure* oldStructure, bool viaGlobalProxy);
    
    static RefPtr<AccessCase> fromStructureStubInfo(VM&, JSCell* owner, CacheableIdentifier, StructureStubInfo&);

    AccessType type() const { return m_type; }
    State state() const { return m_state; }
    PropertyOffset offset() const { return m_offset; }

    Structure* structure() const
    {
        if (m_type == Transition || m_type == Delete || m_type == SetPrivateBrand)
            return m_structureID->previousID();
        return m_structureID.get();
    }
    bool guardedByStructureCheck(const StructureStubInfo&) const;

    Structure* newStructure() const
    {
        ASSERT(m_type == Transition || m_type == Delete || m_type == SetPrivateBrand);
        return m_structureID.get();
    }

    ObjectPropertyConditionSet conditionSet() const { return m_conditionSet; }

    bool hasAlternateBase() const;
    JSObject* alternateBase() const;
    
    WatchpointSet* additionalSet() const;
    bool viaGlobalProxy() const { return m_viaGlobalProxy; }

    // If you supply the optional vector, this will append the set of cells that this will need to keep alive
    // past the call.
    bool doesCalls(VM&, Vector<JSCell*>* cellsToMark = nullptr) const;

    bool isCustom() const
    {
        switch (type()) {
        case CustomValueGetter:
        case CustomAccessorGetter:
        case CustomValueSetter:
        case CustomAccessorSetter:
            return true;
        default:
            return false;
        }
    }

    bool isGetter() const
    {
        switch (type()) {
        case Getter:
        case CustomValueGetter:
        case CustomAccessorGetter:
            return true;
        default:
            return false;
        }
    }

    bool isAccessor() const { return isGetter() || type() == Setter; }

    // Is it still possible for this case to ever be taken? Must call this as a prerequisite for
    // calling generate() and friends. If this returns true, then you can call generate(). If
    // this returns false, then generate() will crash. You must call generate() in the same epoch
    // as when you called couldStillSucceed().
    bool couldStillSucceed() const;

    // If this method returns true, then it's a good idea to remove 'other' from the access once 'this'
    // is added. This method assumes that in case of contradictions, 'this' represents a newer, and so
    // more useful, truth. This method can be conservative; it will return false when it doubt.
    bool canReplace(const AccessCase& other) const;

    void dump(PrintStream& out) const;

    bool usesPolyProto() const
    {
        return !!m_polyProtoAccessChain;
    }

    bool requiresIdentifierNameMatch() const;
    bool requiresInt32PropertyCheck() const;
    bool needsScratchFPR() const;

    UniquedStringImpl* uid() const { return m_identifier.uid(); }
    CacheableIdentifier identifier() const { return m_identifier; }

#if ASSERT_ENABLED
    void checkConsistency(StructureStubInfo&);
#else
    ALWAYS_INLINE void checkConsistency(StructureStubInfo&) { }
#endif

    unsigned hash() const
    {
        return computeHash(m_conditionSet.hash(), static_cast<unsigned>(m_type), m_viaGlobalProxy, m_structureID.unvalidatedGet(), m_offset);
    }

    static bool canBeShared(const AccessCase&, const AccessCase&);

    template<typename Func>
    void runWithDowncast(const Func&);

    void operator delete(AccessCase*, std::destroying_delete_t);

protected:
    AccessCase(VM&, JSCell* owner, AccessType, CacheableIdentifier, PropertyOffset, Structure*, const ObjectPropertyConditionSet&, RefPtr<PolyProtoAccessChain>&&);
    AccessCase(AccessCase&& other)
        : m_type(WTFMove(other.m_type))
        , m_state(WTFMove(other.m_state))
        , m_viaGlobalProxy(WTFMove(other.m_viaGlobalProxy))
        , m_offset(WTFMove(other.m_offset))
        , m_structureID(WTFMove(other.m_structureID))
        , m_conditionSet(WTFMove(other.m_conditionSet))
        , m_polyProtoAccessChain(WTFMove(other.m_polyProtoAccessChain))
        , m_identifier(WTFMove(other.m_identifier))
    { }

    AccessCase(const AccessCase& other)
        : m_type(other.m_type)
        , m_state(other.m_state)
        , m_viaGlobalProxy(other.m_viaGlobalProxy)
        , m_offset(other.m_offset)
        , m_structureID(other.m_structureID)
        , m_conditionSet(other.m_conditionSet)
        , m_polyProtoAccessChain(other.m_polyProtoAccessChain)
        , m_identifier(other.m_identifier)
    { }

    AccessCase& operator=(const AccessCase&) = delete;
    void resetState() { m_state = Primordial; }

    Ref<AccessCase> cloneImpl() const;
    WatchpointSet* additionalSetImpl() const { return nullptr; }
    bool hasAlternateBaseImpl() const;
    void dumpImpl(PrintStream&, CommaPrinter&, Indenter&) const { }
    JSObject* alternateBaseImpl() const;

private:
    friend class CodeBlock;
    friend class PolymorphicAccess;

    friend class ProxyableAccessCase;
    friend class GetterSetterAccessCase;
    friend class IntrinsicGetterAccessCase;
    friend class ModuleNamespaceAccessCase;
    friend class InstanceOfAccessCase;

    template<typename Functor>
    void forEachDependentCell(VM&, const Functor&) const;

    DECLARE_VISIT_AGGREGATE_WITH_MODIFIER(const);
    bool visitWeak(VM&) const;
    template<typename Visitor> void propagateTransitions(Visitor&) const;

    // FIXME: This only exists because of how AccessCase puts post-generation things into itself.
    // https://bugs.webkit.org/show_bug.cgi?id=156456
    Ref<AccessCase> clone() const;

    // Perform any action that must be performed before the end of the epoch in which the case
    // was created. Returns a set of watchpoint sets that will need to be watched.
    Vector<WatchpointSet*, 2> commit(VM&);

    bool guardedByStructureCheckSkippingConstantIdentifierCheck() const;

    AccessType m_type;
    State m_state { Primordial };
protected:
    // m_viaGlobalProxy is true only if the instance inherits (or it is) ProxyableAccessCase.
    // We put this value here instead of ProxyableAccessCase to reduce the size of ProxyableAccessCase and its
    // derived classes, which are super frequently allocated.
    bool m_viaGlobalProxy { false };
private:
    PropertyOffset m_offset;

    // Usually this is the structure that we expect the base object to have. But, this is the *new*
    // structure for a transition and we rely on the fact that it has a strong reference to the old
    // structure. For proxies, this is the structure of the object behind the proxy.
    WriteBarrierStructureID m_structureID;

    ObjectPropertyConditionSet m_conditionSet;

    RefPtr<PolyProtoAccessChain> m_polyProtoAccessChain;

    CacheableIdentifier m_identifier;
};

} // namespace JSC

#endif
