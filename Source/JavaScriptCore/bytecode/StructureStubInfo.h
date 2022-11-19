/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "CacheableIdentifier.h"
#include "CodeBlock.h"
#include "CodeOrigin.h"
#include "Instruction.h"
#include "JITStubRoutine.h"
#include "MacroAssembler.h"
#include "Options.h"
#include "PolymorphicAccess.h"
#include "PutKind.h"
#include "RegisterSet.h"
#include "Structure.h"
#include "StructureSet.h"
#include "StructureStubClearingWatchpoint.h"
#include "StubInfoSummary.h"
#include <wtf/Box.h>
#include <wtf/Lock.h>

namespace JSC {

#if ENABLE(JIT)

namespace DFG {
struct UnlinkedStructureStubInfo;
}

class AccessCase;
class AccessGenerationResult;
class PolymorphicAccess;

enum class AccessType : int8_t {
    GetById,
    GetByIdWithThis,
    GetByIdDirect,
    TryGetById,
    GetByVal,
    GetByValWithThis,
    PutById,
    PutByVal,
    PutPrivateName,
    InById,
    InByVal,
    HasPrivateName,
    HasPrivateBrand,
    InstanceOf,
    DeleteByID,
    DeleteByVal,
    GetPrivateName,
    GetPrivateNameById,
    CheckPrivateBrand,
    SetPrivateBrand,
};

enum class CacheType : int8_t {
    Unset,
    GetByIdSelf,
    PutByIdReplace,
    InByIdSelf,
    Stub,
    ArrayLength,
    StringLength
};

struct UnlinkedStructureStubInfo;
struct BaselineUnlinkedStructureStubInfo;

class StructureStubInfo {
    WTF_MAKE_NONCOPYABLE(StructureStubInfo);
    WTF_MAKE_FAST_ALLOCATED;
public:
    StructureStubInfo(AccessType accessType, CodeOrigin codeOrigin)
        : codeOrigin(codeOrigin)
        , accessType(accessType)
        , bufferingCountdown(Options::repatchBufferingCountdown())
    {
    }

    StructureStubInfo()
        : StructureStubInfo(AccessType::GetById, { })
    { }

    ~StructureStubInfo();

    void initGetByIdSelf(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset, CacheableIdentifier);
    void initArrayLength(const ConcurrentJSLockerBase&);
    void initStringLength(const ConcurrentJSLockerBase&);
    void initPutByIdReplace(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset, CacheableIdentifier);
    void initInByIdSelf(const ConcurrentJSLockerBase&, CodeBlock*, Structure* inlineAccessBaseStructure, PropertyOffset, CacheableIdentifier);

    AccessGenerationResult addAccessCase(const GCSafeConcurrentJSLocker&, JSGlobalObject*, CodeBlock*, ECMAMode, CacheableIdentifier, RefPtr<AccessCase>);

    void reset(const ConcurrentJSLockerBase&, CodeBlock*);

    void deref();
    void aboutToDie();

    void initializeFromUnlinkedStructureStubInfo(const BaselineUnlinkedStructureStubInfo&);
    void initializeFromDFGUnlinkedStructureStubInfo(const DFG::UnlinkedStructureStubInfo&);

    DECLARE_VISIT_AGGREGATE;

    // Check if the stub has weak references that are dead. If it does, then it resets itself,
    // either entirely or just enough to ensure that those dead pointers don't get used anymore.
    void visitWeakReferences(const ConcurrentJSLockerBase&, CodeBlock*);
    
    // This returns true if it has marked everything that it will ever mark.
    template<typename Visitor> void propagateTransitions(Visitor&);
        
    StubInfoSummary summary(VM&) const;
    
    static StubInfoSummary summary(VM&, const StructureStubInfo*);

    CacheableIdentifier identifier()
    {
        switch (m_cacheType) {
        case CacheType::Unset:
        case CacheType::ArrayLength:
        case CacheType::StringLength:
        case CacheType::Stub:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case CacheType::PutByIdReplace:
        case CacheType::InByIdSelf:
        case CacheType::GetByIdSelf:
            break;
        }
        return m_identifier;
    }

    bool containsPC(void* pc) const;

    uint32_t inlineCodeSize() const
    {
        if (useDataIC)
            return 0;
        int32_t inlineSize = MacroAssembler::differenceBetweenCodePtr(startLocation, doneLocation);
        ASSERT(inlineSize >= 0);
        return inlineSize;
    }

    JSValueRegs valueRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            m_valueTagGPR,
#endif
            m_valueGPR);
    }

    JSValueRegs propertyRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            propertyTagGPR(),
#endif
            propertyGPR());
    }

    JSValueRegs baseRegs() const
    {
        return JSValueRegs(
#if USE(JSVALUE32_64)
            m_baseTagGPR,
#endif
            m_baseGPR);
    }

    bool thisValueIsInExtraGPR() const { return accessType == AccessType::GetByIdWithThis || accessType == AccessType::GetByValWithThis; }

#if ASSERT_ENABLED
    void checkConsistency();
#else
    ALWAYS_INLINE void checkConsistency() { }
#endif

    CacheType cacheType() const { return m_cacheType; }

    // Not ByVal and ById case: e.g. instanceof, by-index etc.
    ALWAYS_INLINE bool considerCachingGeneric(VM& vm, CodeBlock* codeBlock, Structure* structure)
    {
        return considerCaching(vm, codeBlock, structure, CacheableIdentifier());
    }

    ALWAYS_INLINE bool considerCachingBy(VM& vm, CodeBlock* codeBlock, Structure* structure, CacheableIdentifier impl)
    {
        return considerCaching(vm, codeBlock, structure, impl);
    }

    Structure* inlineAccessBaseStructure() const
    {
        return m_inlineAccessBaseStructureID.get();
    }
private:
    ALWAYS_INLINE bool considerCaching(VM& vm, CodeBlock* codeBlock, Structure* structure, CacheableIdentifier impl)
    {
        DisallowGC disallowGC;

        // We never cache non-cells.
        if (!structure) {
            sawNonCell = true;
            return false;
        }
        
        // This method is called from the Optimize variants of IC slow paths. The first part of this
        // method tries to determine if the Optimize variant should really behave like the
        // non-Optimize variant and leave the IC untouched.
        //
        // If we determine that we should do something to the IC then the next order of business is
        // to determine if this Structure would impact the IC at all. We know that it won't, if we
        // have already buffered something on its behalf. That's what the m_bufferedStructures set is
        // for.
        
        everConsidered = true;
        if (!countdown) {
            // Check if we have been doing repatching too frequently. If so, then we should cool off
            // for a while.
            WTF::incrementWithSaturation(repatchCount);
            if (repatchCount > Options::repatchCountForCoolDown()) {
                // We've been repatching too much, so don't do it now.
                repatchCount = 0;
                // The amount of time we require for cool-down depends on the number of times we've
                // had to cool down in the past. The relationship is exponential. The max value we
                // allow here is 2^256 - 2, since the slow paths may increment the count to indicate
                // that they'd like to temporarily skip patching just this once.
                countdown = WTF::leftShiftWithSaturation(
                    static_cast<uint8_t>(Options::initialCoolDownCount()),
                    numberOfCoolDowns,
                    static_cast<uint8_t>(std::numeric_limits<uint8_t>::max() - 1));
                WTF::incrementWithSaturation(numberOfCoolDowns);
                
                // We may still have had something buffered. Trigger generation now.
                bufferingCountdown = 0;
                return true;
            }
            
            // We don't want to return false due to buffering indefinitely.
            if (!bufferingCountdown) {
                // Note that when this returns true, it's possible that we will not even get an
                // AccessCase because this may cause Repatch.cpp to simply do an in-place
                // repatching.
                return true;
            }
            
            bufferingCountdown--;
            
            // Now protect the IC buffering. We want to proceed only if this is a structure that
            // we don't already have a case buffered for. Note that if this returns true but the
            // bufferingCountdown is not zero then we will buffer the access case for later without
            // immediately generating code for it.
            //
            // NOTE: This will behave oddly for InstanceOf if the user varies the prototype but not
            // the base's structure. That seems unlikely for the canonical use of instanceof, where
            // the prototype is fixed.
            bool isNewlyAdded = false;
            {
                Locker locker { m_bufferedStructuresLock };
                isNewlyAdded = m_bufferedStructures.add({ structure, impl }).isNewEntry;
            }
            if (isNewlyAdded)
                vm.writeBarrier(codeBlock);
            return isNewlyAdded;
        }
        countdown--;
        return false;
    }

    void setCacheType(const ConcurrentJSLockerBase&, CacheType);

    void clearBufferedStructures()
    {
        Locker locker { m_bufferedStructuresLock };
        m_bufferedStructures.clear();
    }

    class BufferedStructure {
    public:
        static constexpr uintptr_t hashTableDeletedValue = 0x2;
        BufferedStructure() = default;
        BufferedStructure(Structure* structure, CacheableIdentifier byValId)
            : m_structure(structure)
            , m_byValId(byValId)
        { }
        BufferedStructure(WTF::HashTableDeletedValueType)
            : m_structure(bitwise_cast<Structure*>(hashTableDeletedValue))
        { }

        bool isHashTableDeletedValue() const { return bitwise_cast<uintptr_t>(m_structure) == hashTableDeletedValue; }

        unsigned hash() const
        {
            unsigned hash = PtrHash<Structure*>::hash(m_structure);
            if (m_byValId)
                hash += m_byValId.hash();
            return hash;
        }

        friend bool operator==(const BufferedStructure& a, const BufferedStructure& b)
        {
            return a.m_structure == b.m_structure && a.m_byValId == b.m_byValId;
        }

        friend bool operator!=(const BufferedStructure& a, const BufferedStructure& b)
        {
            return !(a == b);
        }

        struct Hash {
            static unsigned hash(const BufferedStructure& key)
            {
                return key.hash();
            }

            static bool equal(const BufferedStructure& a, const BufferedStructure& b)
            {
                return a == b;
            }

            static constexpr bool safeToCompareToEmptyOrDeleted = false;
        };
        using KeyTraits = SimpleClassHashTraits<BufferedStructure>;
        static_assert(KeyTraits::emptyValueIsZero, "Structure* and CacheableIdentifier are empty if they are zero-initialized");

        Structure* structure() const { return m_structure; }
        const CacheableIdentifier& byValId() const { return m_byValId; }

    private:
        Structure* m_structure { nullptr };
        CacheableIdentifier m_byValId;
    };

public:
    static ptrdiff_t offsetOfByIdSelfOffset() { return OBJECT_OFFSETOF(StructureStubInfo, byIdSelfOffset); }
    static ptrdiff_t offsetOfInlineAccessBaseStructureID() { return OBJECT_OFFSETOF(StructureStubInfo, m_inlineAccessBaseStructureID); }
    static ptrdiff_t offsetOfCodePtr() { return OBJECT_OFFSETOF(StructureStubInfo, m_codePtr); }
    static ptrdiff_t offsetOfDoneLocation() { return OBJECT_OFFSETOF(StructureStubInfo, doneLocation); }
    static ptrdiff_t offsetOfSlowPathStartLocation() { return OBJECT_OFFSETOF(StructureStubInfo, slowPathStartLocation); }
    static ptrdiff_t offsetOfSlowOperation() { return OBJECT_OFFSETOF(StructureStubInfo, m_slowOperation); }
    static ptrdiff_t offsetOfCountdown() { return OBJECT_OFFSETOF(StructureStubInfo, countdown); }

    GPRReg thisGPR() const { return m_extraGPR; }
    GPRReg prototypeGPR() const { return m_extraGPR; }
    GPRReg brandGPR() const { return m_extraGPR; }
    GPRReg propertyGPR() const
    {
        switch (accessType) {
        case AccessType::GetByValWithThis:
            return m_extra2GPR;
        default:
            return m_extraGPR;
        }
    }

#if USE(JSVALUE32_64)
    GPRReg thisTagGPR() const { return m_extraTagGPR; }
    GPRReg prototypeTagGPR() const { return m_extraTagGPR; }
    GPRReg propertyTagGPR() const
    {
        switch (accessType) {
        case AccessType::GetByValWithThis:
            return m_extra2TagGPR;
        default:
            return m_extraTagGPR;
        }
    }
#endif

    CodeOrigin codeOrigin { };
    PropertyOffset byIdSelfOffset;
    WriteBarrierStructureID m_inlineAccessBaseStructureID;
    std::unique_ptr<PolymorphicAccess> m_stub;
private:
    CacheableIdentifier m_identifier;
    // Represents those structures that already have buffered AccessCases in the PolymorphicAccess.
    // Note that it's always safe to clear this. If we clear it prematurely, then if we see the same
    // structure again during this buffering countdown, we will create an AccessCase object for it.
    // That's not so bad - we'll get rid of the redundant ones once we regenerate.
    HashSet<BufferedStructure, BufferedStructure::Hash, BufferedStructure::KeyTraits> m_bufferedStructures WTF_GUARDED_BY_LOCK(m_bufferedStructuresLock);
public:
    // This is either the start of the inline IC for *byId caches. or the location of patchable jump for 'instanceof' caches.
    // If useDataIC is true, then it is nullptr.
    CodeLocationLabel<JITStubRoutinePtrTag> startLocation;
    CodeLocationLabel<JSInternalPtrTag> doneLocation;
    CodeLocationLabel<JITStubRoutinePtrTag> slowPathStartLocation;

    union {
        CodeLocationCall<JSInternalPtrTag> m_slowPathCallLocation;
        CodePtr<OperationPtrTag> m_slowOperation;
    };

    CodePtr<JITStubRoutinePtrTag> m_codePtr;

    ScalarRegisterSet usedRegisters;

    CallSiteIndex callSiteIndex;

    GPRReg m_baseGPR { InvalidGPRReg };
    GPRReg m_valueGPR { InvalidGPRReg };
    GPRReg m_extraGPR { InvalidGPRReg };
    GPRReg m_extra2GPR { InvalidGPRReg };
    GPRReg m_stubInfoGPR { InvalidGPRReg };
    GPRReg m_arrayProfileGPR { InvalidGPRReg };
#if USE(JSVALUE32_64)
    GPRReg m_valueTagGPR { InvalidGPRReg };
    // FIXME: [32-bits] Check if StructureStubInfo::m_baseTagGPR is used somewhere.
    // https://bugs.webkit.org/show_bug.cgi?id=204726
    GPRReg m_baseTagGPR { InvalidGPRReg };
    GPRReg m_extraTagGPR { InvalidGPRReg };
    GPRReg m_extra2TagGPR { InvalidGPRReg };
#endif

    AccessType accessType { AccessType::GetById };
private:
    CacheType m_cacheType { CacheType::Unset };
public:
    // We repatch only when this is zero. If not zero, we decrement.
    // Setting 1 for a totally clear stub, we'll patch it after the first execution.
    uint8_t countdown { 1 };
    uint8_t repatchCount { 0 };
    uint8_t numberOfCoolDowns { 0 };
    uint8_t bufferingCountdown;
private:
    Lock m_bufferedStructuresLock;
public:
    bool resetByGC : 1 { false };
    bool tookSlowPath : 1 { false };
    bool everConsidered : 1 { false };
    bool prototypeIsKnownObject : 1 { false }; // Only relevant for InstanceOf.
    bool sawNonCell : 1 { false };
    bool hasConstantIdentifier : 1 { true };
    bool propertyIsString : 1 { false };
    bool propertyIsInt32 : 1 { false };
    bool propertyIsSymbol : 1 { false };
    bool useDataIC : 1 { false };
};

inline CodeOrigin getStructureStubInfoCodeOrigin(StructureStubInfo& structureStubInfo)
{
    return structureStubInfo.codeOrigin;
}

inline auto appropriateOptimizingGetByIdFunction(AccessType type) -> decltype(&operationGetByIdOptimize)
{
    switch (type) {
    case AccessType::GetById:
        return operationGetByIdOptimize;
    case AccessType::TryGetById:
        return operationTryGetByIdOptimize;
    case AccessType::GetByIdDirect:
        return operationGetByIdDirectOptimize;
    case AccessType::GetPrivateNameById:
        return operationGetPrivateNameByIdOptimize;
    case AccessType::GetByIdWithThis:
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

inline auto appropriateGenericGetByIdFunction(AccessType type) -> decltype(&operationGetByIdGeneric)
{
    switch (type) {
    case AccessType::GetById:
        return operationGetByIdGeneric;
    case AccessType::TryGetById:
        return operationTryGetByIdGeneric;
    case AccessType::GetByIdDirect:
        return operationGetByIdDirectGeneric;
    case AccessType::GetPrivateNameById:
        return operationGetPrivateNameByIdGeneric;
    case AccessType::GetByIdWithThis:
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

struct UnlinkedStructureStubInfo {
    AccessType accessType;
    PutKind putKind { PutKind::Direct };
    PrivateFieldPutKind privateFieldPutKind { PrivateFieldPutKind::none() };
    ECMAMode ecmaMode { ECMAMode::sloppy() };
    bool propertyIsInt32 : 1 { false };
    bool propertyIsString : 1 { false };
    bool propertyIsSymbol : 1 { false };
    bool prototypeIsKnownObject : 1 { false };
    bool tookSlowPath : 1 { false };
    CodeLocationLabel<JSInternalPtrTag> doneLocation;
    CodeLocationLabel<JITStubRoutinePtrTag> slowPathStartLocation;
};

struct BaselineUnlinkedStructureStubInfo : UnlinkedStructureStubInfo {
    BytecodeIndex bytecodeIndex;
};

#else

class StructureStubInfo;

#endif // ENABLE(JIT)

typedef HashMap<CodeOrigin, StructureStubInfo*, CodeOriginApproximateHash> StubInfoMap;

} // namespace JSC
