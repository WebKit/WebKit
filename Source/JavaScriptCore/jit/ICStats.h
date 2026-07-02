/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "CacheableIdentifierInlines.h"
#include "ClassInfo.h"
#include "Identifier.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/Spectrum.h>
#include <wtf/TZoneMalloc.h>

namespace JSC {

namespace ICStatsInternal {
static constexpr bool traceHandlerChains = false;
}

#define FOR_EACH_ICEVENT_KIND(macro) \
    macro(InvalidKind) \
    macro(GetByAddAccessCase) \
    macro(GetByReplaceWithJump) \
    macro(GetBySelfPatch) \
    macro(InAddAccessCase) \
    macro(InReplaceWithJump) \
    macro(InReplaceWithGeneric) \
    macro(InstanceOfAddAccessCase) \
    macro(InstanceOfReplaceWithJump) \
    macro(OperationGetByIdGaveUp) \
    macro(OperationGetByIdGeneric) \
    macro(OperationGetByIdBuildList) \
    macro(OperationGetByIdOptimize) \
    macro(OperationGetByValOptimize) \
    macro(OperationGetByIdWithThisOptimize) \
    macro(OperationGetByIdWithThisGaveUp) \
    macro(OperationGetByIdWithThisGeneric) \
    macro(OperationGetByValWithThisOptimize) \
    macro(OperationGenericIn) \
    macro(OperationInByIdGeneric) \
    macro(OperationInByIdOptimize) \
    macro(OperationPutByIdStrictGaveUp) \
    macro(OperationPutByIdSloppyGaveUp) \
    macro(OperationPutByIdDirectStrictGaveUp) \
    macro(OperationPutByIdDirectSloppyGaveUp) \
    macro(OperationPutByIdStrictOptimize) \
    macro(OperationPutByIdSloppyOptimize) \
    macro(OperationPutByIdDirectStrictOptimize) \
    macro(OperationPutByIdDirectSloppyOptimize) \
    macro(OperationPutByIdStrictBuildList) \
    macro(OperationPutByIdSloppyBuildList) \
    macro(OperationPutByIdDefinePrivateFieldStrictOptimize) \
    macro(OperationPutByIdPutPrivateFieldStrictOptimize) \
    macro(PutByAddAccessCase) \
    macro(PutByReplaceWithJump) \
    macro(PutBySelfPatch) \
    macro(InBySelfPatch) \
    macro(DelByReplaceWithJump) \
    macro(DelByReplaceWithGeneric) \
    macro(OperationGetPrivateNameOptimize) \
    macro(OperationGetPrivateNameById) \
    macro(OperationGetPrivateNameByIdOptimize) \
    macro(OperationGetPrivateNameByIdGeneric) \
    macro(CheckPrivateBrandAddAccessCase) \
    macro(SetPrivateBrandAddAccessCase) \
    macro(CheckPrivateBrandReplaceWithJump) \
    macro(SetPrivateBrandReplaceWithJump) \
    macro(OperationPutByIdSetPrivateFieldStrictOptimize) \
    macro(OperationPutByIdDefinePrivateFieldStrictGaveUp) \
    macro(OperationPutByIdSetPrivateFieldStrictGaveUp) \
    macro(OperationGetByValGaveUp) \
    macro(OperationGetByValGeneric) \
    macro(OperationGetByValWithThisGaveUp) \
    macro(OperationGetByValWithThisGeneric) \
    macro(OperationPutByValStrictGaveUp) \
    macro(OperationPutByValStrictGeneric) \
    macro(OperationPutByValSloppyGaveUp) \
    macro(OperationPutByValSloppyGeneric) \
    macro(OperationPutByValDefinePrivateFieldGaveUp) \
    macro(OperationPutByValDefinePrivateFieldGeneric) \
    macro(OperationPutByValSetPrivateFieldGaveUp) \
    macro(OperationPutByValSetPrivateFieldGeneric) \
    /* JIT execution tracing events */ \
    /* Slow path handlers */ \
    macro(GetByIdSlowPath) \
    macro(GetByIdWithThisSlowPath) \
    macro(GetByValSlowPath) \
    macro(GetPrivateNameSlowPath) \
    macro(GetByValWithThisSlowPath) \
    macro(PutByIdSlowPath) \
    macro(PutByValSlowPath) \
    macro(InstanceOfSlowPath) \
    macro(DeleteByIdSlowPath) \
    macro(DeleteByValSlowPath) \
    /* GetById handlers */ \
    macro(GetByIdLoadOwnPropertyHandler) \
    macro(GetByIdLoadPrototypePropertyHandler) \
    macro(GetByIdMissHandler) \
    macro(GetByIdCustomAccessorHandler) \
    macro(GetByIdCustomValueHandler) \
    macro(GetByIdGetterHandler) \
    macro(GetByIdProxyObjectLoadHandler) \
    macro(GetByIdModuleNamespaceLoadHandler) \
    /* PutById handlers */ \
    macro(PutByIdReplaceHandler) \
    macro(PutByIdTransitionHandler) \
    macro(PutByIdTransitionReallocatingOutOfLineHandler) \
    macro(PutByIdCustomAccessorHandler) \
    macro(PutByIdCustomValueHandler) \
    macro(PutByIdStrictSetterHandler) \
    macro(PutByIdSloppySetterHandler) \
    /* InById handlers */ \
    macro(InByIdHitHandler) \
    macro(InByIdMissHandler) \
    /* DeleteById handlers */ \
    macro(DeleteByIdDeleteHandler) \
    macro(DeleteByIdMissHandler) \
    macro(DeleteByIdNonConfigurableHandler) \
    /* InstanceOf handlers */ \
    macro(InstanceOfHitHandler) \
    macro(InstanceOfMissHandler) \
    /* GetByVal handlers */ \
    macro(GetByValLoadOwnPropertyHandler) \
    macro(GetByValLoadPrototypePropertyHandler) \
    macro(GetByValMissHandler) \
    macro(GetByValCustomAccessorHandler) \
    macro(GetByValCustomValueHandler) \
    macro(GetByValGetterHandler) \
    /* PutByVal handlers */ \
    macro(PutByValReplaceHandler) \
    macro(PutByValTransitionHandler) \
    macro(PutByValTransitionOutOfLineHandler) \
    macro(PutByValCustomAccessorHandler) \
    macro(PutByValCustomValueHandler) \
    macro(PutByValStrictSetterHandler) \
    macro(PutByValSloppySetterHandler) \
    /* InByVal handlers */ \
    macro(InByValHitHandler) \
    macro(InByValMissHandler) \
    /* DeleteByVal handlers */ \
    macro(DeleteByValDeleteHandler) \
    macro(DeleteByValMissHandler) \
    macro(DeleteByValNonConfigurableHandler) \
    /* Other handlers */ \
    macro(CheckPrivateBrandHandler) \
    macro(SetPrivateBrandHandler) \
    macro(CompiledHandler) \
    macro(DOMJITHandler)

class ICEvent {
public:
    enum Kind {
#define ICEVENT_KIND_DECLARATION(name) name,
        FOR_EACH_ICEVENT_KIND(ICEVENT_KIND_DECLARATION)
#undef ICEVENT_KIND_DECLARATION
    };

    enum PropertyLocation {
        Unknown,
        BaseObject,
        ProtoLookup
    };

    ICEvent() = default;

    explicit ICEvent(Kind kind)
        : m_kind(kind)
        , m_propertyLocation(Unknown)
    {
        ASSERT(kind != InvalidKind);
    }

    ICEvent(Kind kind, const ClassInfo* classInfo)
        : m_kind(kind)
        , m_classInfo(classInfo)
        , m_propertyLocation(Unknown)
    {
        ASSERT(kind != InvalidKind);
    }

    ICEvent(Kind kind, const ClassInfo* classInfo, bool isBaseProperty)
        : m_kind(kind)
        , m_classInfo(classInfo)
        , m_propertyLocation(isBaseProperty ? BaseObject : ProtoLookup)
    {
        ASSERT(kind != InvalidKind);
    }
    
    ICEvent(WTF::HashTableDeletedValueType)
        : m_kind(InvalidKind)
        , m_propertyLocation(BaseObject)
    {
    }
    
    bool NODELETE operator==(const ICEvent& other) const
    {
        return m_kind == other.m_kind
            && m_classInfo == other.m_classInfo
            && m_propertyLocation == other.m_propertyLocation;
    }
    
    bool operator<(const ICEvent& other) const;
    bool operator>(const ICEvent& other) const { return other < *this; }
    bool operator<=(const ICEvent& other) const { return !(*this > other); }
    bool operator>=(const ICEvent& other) const { return !(*this < other); }

    explicit operator bool() const
    {
        return *this != ICEvent();
    }
    
    Kind kind() const { return m_kind; }
    const ClassInfo* classInfo() const { return m_classInfo; }
    
    unsigned NODELETE hash() const
    {
        return static_cast<unsigned>(m_kind) + static_cast<unsigned>(m_propertyLocation) + WTF::PtrHash<const ClassInfo*>::hash(m_classInfo);
    }
    
    bool NODELETE isHashTableDeletedValue() const
    {
        return *this == ICEvent(WTF::HashTableDeletedValue);
    }

    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

    void dump(PrintStream&) const;
    
    void log() const;
    
private:
    
    Kind m_kind { InvalidKind };
    const ClassInfo* m_classInfo { nullptr };
    PropertyLocation m_propertyLocation { Unknown };
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::ICEvent::Kind);

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::ICEvent> : SimpleClassHashTraits<JSC::ICEvent> {
    static constexpr bool emptyValueIsZero = false;
};

} // namespace WTF

namespace JSC {

class ICStats {
    WTF_MAKE_NONCOPYABLE(ICStats);
    WTF_MAKE_TZONE_ALLOCATED(ICStats);
public:
    ICStats();
    ~ICStats();
    
    void add(const ICEvent& event);
    
    static ICStats& singleton();

    void startNewChain(unsigned totalNumberOfHandlersInChain);
    void appendToCurrentChain(ICEvent::Kind);
    void dumpChains();

private:

    Spectrum<ICEvent, uint64_t> m_spectrum;
    RefPtr<Thread> m_thread;
    Lock m_lock;
    Condition m_condition;
    bool m_shouldStop { false };

    static Atomic<ICStats*> s_instance;
};

#define LOG_IC(arguments) do {                  \
        if (Options::useICStats())              \
            (ICEvent arguments).log();          \
    } while (false)

} // namespace JSC
