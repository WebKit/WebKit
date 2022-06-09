/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "ClassInfo.h"
#include "Identifier.h"
#include <wtf/Condition.h>
#include <wtf/FastMalloc.h>
#include <wtf/Lock.h>
#include <wtf/Noncopyable.h>
#include <wtf/PrintStream.h>
#include <wtf/Spectrum.h>

namespace JSC {

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
    macro(OperationGetById) \
    macro(OperationGetByIdGeneric) \
    macro(OperationGetByIdBuildList) \
    macro(OperationGetByIdOptimize) \
    macro(OperationGetByValOptimize) \
    macro(OperationGetByIdWithThisOptimize) \
    macro(OperationGenericIn) \
    macro(OperationInByIdGeneric) \
    macro(OperationInByIdOptimize) \
    macro(OperationPutByIdStrict) \
    macro(OperationPutByIdNonStrict) \
    macro(OperationPutByIdDirectStrict) \
    macro(OperationPutByIdDirectNonStrict) \
    macro(OperationPutByIdStrictOptimize) \
    macro(OperationPutByIdNonStrictOptimize) \
    macro(OperationPutByIdDirectStrictOptimize) \
    macro(OperationPutByIdDirectNonStrictOptimize) \
    macro(OperationPutByIdStrictBuildList) \
    macro(OperationPutByIdNonStrictBuildList) \
    macro(OperationPutByIdDefinePrivateFieldFieldStrictOptimize) \
    macro(OperationPutByIdPutPrivateFieldFieldStrictOptimize) \
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
    macro(SetPrivateBrandReplaceWithJump)

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

    ICEvent()
    {
    }
    
    ICEvent(Kind kind, const ClassInfo* classInfo, const Identifier propertyName)
        : m_kind(kind)
        , m_classInfo(classInfo)
        , m_propertyName(propertyName)
        , m_propertyLocation(Unknown)
    {
    }

    ICEvent(Kind kind, const ClassInfo* classInfo, const Identifier propertyName, bool isBaseProperty)
        : m_kind(kind)
        , m_classInfo(classInfo)
        , m_propertyName(propertyName)
        , m_propertyLocation(isBaseProperty ? BaseObject : ProtoLookup)
    {
    }
    
    ICEvent(WTF::HashTableDeletedValueType)
        : m_kind(OperationGetById)
    {
    }
    
    bool operator==(const ICEvent& other) const
    {
        return m_kind == other.m_kind
            && m_classInfo == other.m_classInfo
            && m_propertyName == other.m_propertyName;
    }
    
    bool operator!=(const ICEvent& other) const
    {
        return !(*this == other);
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
    const Identifier& propertyName() const { return m_propertyName; }
    
    unsigned hash() const
    {
        if (m_propertyName.isNull())
            return m_kind + m_propertyLocation + WTF::PtrHash<const ClassInfo*>::hash(m_classInfo);
        return m_kind + m_propertyLocation + WTF::PtrHash<const ClassInfo*>::hash(m_classInfo) + StringHash::hash(m_propertyName.string());
    }
    
    bool isHashTableDeletedValue() const
    {
        return *this == ICEvent(WTF::HashTableDeletedValue);
    }
    
    void dump(PrintStream&) const;
    
    void log() const;
    
private:
    
    Kind m_kind { InvalidKind };
    const ClassInfo* m_classInfo { nullptr };
    Identifier m_propertyName;
    PropertyLocation m_propertyLocation;
};

struct ICEventHash {
    static unsigned hash(const ICEvent& key) { return key.hash(); }
    static bool equal(const ICEvent& a, const ICEvent& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::ICEvent::Kind);

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::ICEvent> : JSC::ICEventHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::ICEvent> : SimpleClassHashTraits<JSC::ICEvent> {
    static constexpr bool emptyValueIsZero = false;
};

} // namespace WTF

namespace JSC {

class ICStats {
    WTF_MAKE_NONCOPYABLE(ICStats);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ICStats();
    ~ICStats();
    
    void add(const ICEvent& event);
    
    static ICStats& instance();
    
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
