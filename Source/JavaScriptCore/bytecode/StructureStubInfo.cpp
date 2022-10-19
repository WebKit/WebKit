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

#include "config.h"
#include "StructureStubInfo.h"

#include "BaselineJITRegisters.h"
#include "CacheableIdentifierInlines.h"
#include "DFGJITCode.h"
#include "PolymorphicAccess.h"
#include "Repatch.h"

namespace JSC {

#if ENABLE(JIT)

namespace StructureStubInfoInternal {
static constexpr bool verbose = false;
}

StructureStubInfo::~StructureStubInfo() = default;

void StructureStubInfo::initGetByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier);
    setCacheType(locker, CacheType::GetByIdSelf);
    m_identifier = identifier;
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void StructureStubInfo::initArrayLength(const ConcurrentJSLockerBase& locker)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::ArrayLength);
}

void StructureStubInfo::initStringLength(const ConcurrentJSLockerBase& locker)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::StringLength);
}

void StructureStubInfo::initPutByIdReplace(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::PutByIdReplace);
    m_identifier = identifier;
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void StructureStubInfo::initInByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::InByIdSelf);
    m_identifier = identifier;
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void StructureStubInfo::deref()
{
    switch (m_cacheType) {
    case CacheType::Stub:
        m_stub.reset();
        return;
    case CacheType::Unset:
    case CacheType::GetByIdSelf:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
    case CacheType::ArrayLength:
    case CacheType::StringLength:
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

void StructureStubInfo::aboutToDie()
{
    switch (m_cacheType) {
    case CacheType::Stub:
        m_stub->aboutToDie();
        return;
    case CacheType::Unset:
    case CacheType::GetByIdSelf:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
    case CacheType::ArrayLength:
    case CacheType::StringLength:
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

AccessGenerationResult StructureStubInfo::addAccessCase(
    const GCSafeConcurrentJSLocker& locker, JSGlobalObject* globalObject, CodeBlock* codeBlock, ECMAMode ecmaMode, CacheableIdentifier ident, RefPtr<AccessCase> accessCase)
{
    checkConsistency();

    VM& vm = codeBlock->vm();
    ASSERT(vm.heap.isDeferred());
    AccessGenerationResult result = ([&] () -> AccessGenerationResult {
        if (StructureStubInfoInternal::verbose)
            dataLog("Adding access case: ", accessCase, "\n");
        
        if (!accessCase)
            return AccessGenerationResult::GaveUp;
        
        AccessGenerationResult result;
        
        if (m_cacheType == CacheType::Stub) {
            result = m_stub->addCase(locker, vm, codeBlock, *this, accessCase.releaseNonNull());
            
            if (StructureStubInfoInternal::verbose)
                dataLog("Had stub, result: ", result, "\n");

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                clearBufferedStructures();
                return result;
            }
        } else {
            std::unique_ptr<PolymorphicAccess> access = makeUnique<PolymorphicAccess>();
            
            Vector<RefPtr<AccessCase>, 2> accessCases;
            
            auto previousCase = AccessCase::fromStructureStubInfo(vm, codeBlock, ident, *this);
            if (previousCase)
                accessCases.append(WTFMove(previousCase));
            
            accessCases.append(WTFMove(accessCase));
            
            result = access->addCases(locker, vm, codeBlock, *this, WTFMove(accessCases));
            
            if (StructureStubInfoInternal::verbose)
                dataLog("Created stub, result: ", result, "\n");

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                clearBufferedStructures();
                return result;
            }
            
            setCacheType(locker, CacheType::Stub);
            m_stub = WTFMove(access);
        }
        
        ASSERT(m_cacheType == CacheType::Stub);
        RELEASE_ASSERT(!result.generatedSomeCode());
        
        // If we didn't buffer any cases then bail. If this made no changes then we'll just try again
        // subject to cool-down.
        if (!result.buffered()) {
            if (StructureStubInfoInternal::verbose)
                dataLog("Didn't buffer anything, bailing.\n");
            clearBufferedStructures();
            return result;
        }
        
        // The buffering countdown tells us if we should be repatching now.
        if (bufferingCountdown) {
            if (StructureStubInfoInternal::verbose)
                dataLog("Countdown is too high: ", bufferingCountdown, ".\n");
            return result;
        }
        
        // Forget the buffered structures so that all future attempts to cache get fully handled by the
        // PolymorphicAccess.
        clearBufferedStructures();
        
        result = m_stub->regenerate(locker, vm, globalObject, codeBlock, ecmaMode, *this);
        
        if (StructureStubInfoInternal::verbose)
            dataLog("Regeneration result: ", result, "\n");
        
        RELEASE_ASSERT(!result.buffered());
        
        if (!result.generatedSomeCode())
            return result;

        // When we first transition to becoming a Stub, we might still be running the inline
        // access code. That's because when we first transition to becoming a Stub, we may
        // be buffered, and we have not yet generated any code. Once the Stub finally generates
        // code, we're no longer running the inline access code, so we can then clear out
        // m_inlineAccessBaseStructureID. The reason we don't clear m_inlineAccessBaseStructureID while
        // we're buffered is because we rely on it to reset during GC if m_inlineAccessBaseStructureID
        // is collected.
        m_identifier = nullptr;
        m_inlineAccessBaseStructureID.clear();
        
        // If we generated some code then we don't want to attempt to repatch in the future until we
        // gather enough cases.
        bufferingCountdown = Options::repatchBufferingCountdown();
        return result;
    })();
    vm.writeBarrier(codeBlock);
    return result;
}

void StructureStubInfo::reset(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    clearBufferedStructures();
    m_identifier = nullptr;
    m_inlineAccessBaseStructureID.clear();

    if (m_cacheType == CacheType::Unset)
        return;

    if (Options::verboseOSR()) {
        // This can be called from GC destructor calls, so we don't try to do a full dump
        // of the CodeBlock.
        dataLog("Clearing structure cache (kind ", static_cast<int>(accessType), ") in ", RawPointer(codeBlock), ".\n");
    }

    switch (accessType) {
    case AccessType::TryGetById:
        resetGetBy(codeBlock, *this, GetByKind::TryById);
        break;
    case AccessType::GetById:
        resetGetBy(codeBlock, *this, GetByKind::ById);
        break;
    case AccessType::GetByIdWithThis:
        resetGetBy(codeBlock, *this, GetByKind::ByIdWithThis);
        break;
    case AccessType::GetByIdDirect:
        resetGetBy(codeBlock, *this, GetByKind::ByIdDirect);
        break;
    case AccessType::GetByVal:
        resetGetBy(codeBlock, *this, GetByKind::ByVal);
        break;
    case AccessType::GetByValWithThis:
        resetGetBy(codeBlock, *this, GetByKind::ByValWithThis);
        break;
    case AccessType::GetPrivateName:
        resetGetBy(codeBlock, *this, GetByKind::PrivateName);
        break;
    case AccessType::GetPrivateNameById:
        resetGetBy(codeBlock, *this, GetByKind::PrivateNameById);
        break;
    case AccessType::PutById:
        resetPutBy(codeBlock, *this, PutByKind::ById);
        break;
    case AccessType::PutByVal:
    case AccessType::PutPrivateName:
        resetPutBy(codeBlock, *this, PutByKind::ByVal);
        break;
    case AccessType::InById:
        resetInBy(codeBlock, *this, InByKind::ById);
        break;
    case AccessType::InByVal:
        resetInBy(codeBlock, *this, InByKind::ByVal);
        break;
    case AccessType::HasPrivateName:
        resetInBy(codeBlock, *this, InByKind::PrivateName);
        break;
    case AccessType::HasPrivateBrand:
        resetHasPrivateBrand(codeBlock, *this);
        break;
    case AccessType::InstanceOf:
        resetInstanceOf(codeBlock, *this);
        break;
    case AccessType::DeleteByID:
        resetDelBy(codeBlock, *this, DelByKind::ById);
        break;
    case AccessType::DeleteByVal:
        resetDelBy(codeBlock, *this, DelByKind::ByVal);
        break;
    case AccessType::CheckPrivateBrand:
        resetCheckPrivateBrand(codeBlock, *this);
        break;
    case AccessType::SetPrivateBrand:
        resetSetPrivateBrand(codeBlock, *this);
        break;
    }
    
    deref();
    setCacheType(locker, CacheType::Unset);
}

template<typename Visitor>
void StructureStubInfo::visitAggregateImpl(Visitor& visitor)
{
    {
        Locker locker { m_bufferedStructuresLock };
        for (auto& bufferedStructure : m_bufferedStructures)
            bufferedStructure.byValId().visitAggregate(visitor);
    }
    m_identifier.visitAggregate(visitor);
    switch (m_cacheType) {
    case CacheType::Unset:
    case CacheType::ArrayLength:
    case CacheType::StringLength:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
    case CacheType::GetByIdSelf:
        return;
    case CacheType::Stub:
        m_stub->visitAggregate(visitor);
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return;
}

DEFINE_VISIT_AGGREGATE(StructureStubInfo);

void StructureStubInfo::visitWeakReferences(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    VM& vm = codeBlock->vm();
    {
        Locker locker { m_bufferedStructuresLock };
        m_bufferedStructures.removeIf(
            [&] (auto& entry) -> bool {
                return !vm.heap.isMarked(entry.structure());
            });
    }

    bool isValid = true;
    if (Structure* structure = inlineAccessBaseStructure())
        isValid &= vm.heap.isMarked(structure);
    if (m_cacheType == CacheType::Stub)
        isValid &= m_stub->visitWeak(vm);

    if (isValid)
        return;

    reset(locker, codeBlock);
    resetByGC = true;
}

template<typename Visitor>
void StructureStubInfo::propagateTransitions(Visitor& visitor)
{
    if (Structure* structure = inlineAccessBaseStructure())
        structure->markIfCheap(visitor);

    if (m_cacheType == CacheType::Stub)
        m_stub->propagateTransitions(visitor);
}

template void StructureStubInfo::propagateTransitions(AbstractSlotVisitor&);
template void StructureStubInfo::propagateTransitions(SlotVisitor&);

StubInfoSummary StructureStubInfo::summary(VM& vm) const
{
    StubInfoSummary takesSlowPath = StubInfoSummary::TakesSlowPath;
    StubInfoSummary simple = StubInfoSummary::Simple;
    if (m_cacheType == CacheType::Stub) {
        PolymorphicAccess* list = m_stub.get();
        for (unsigned i = 0; i < list->size(); ++i) {
            const AccessCase& access = list->at(i);
            if (access.doesCalls(vm)) {
                takesSlowPath = StubInfoSummary::TakesSlowPathAndMakesCalls;
                simple = StubInfoSummary::MakesCalls;
                break;
            }
        }
    }
    
    if (tookSlowPath || sawNonCell)
        return takesSlowPath;
    
    if (!everConsidered)
        return StubInfoSummary::NoInformation;
    
    return simple;
}

StubInfoSummary StructureStubInfo::summary(VM& vm, const StructureStubInfo* stubInfo)
{
    if (!stubInfo)
        return StubInfoSummary::NoInformation;
    
    return stubInfo->summary(vm);
}

bool StructureStubInfo::containsPC(void* pc) const
{
    if (m_cacheType != CacheType::Stub)
        return false;
    return m_stub->containsPC(pc);
}

ALWAYS_INLINE void StructureStubInfo::setCacheType(const ConcurrentJSLockerBase&, CacheType newCacheType)
{
    m_cacheType = newCacheType;
}

static CodePtr<OperationPtrTag> slowOperationFromUnlinkedStructureStubInfo(const UnlinkedStructureStubInfo& unlinkedStubInfo)
{
    switch (unlinkedStubInfo.accessType) {
    case AccessType::DeleteByVal:
        return operationDeleteByValOptimize;
    case AccessType::DeleteByID:
        return operationDeleteByIdOptimize;
    case AccessType::GetByVal:
        return operationGetByValOptimize;
    case AccessType::InstanceOf:
        return operationInstanceOfOptimize;
    case AccessType::InByVal:
        return operationInByValOptimize;
    case AccessType::InById:
        return operationInByIdOptimize;
    case AccessType::GetById:
        return operationGetByIdOptimize;
    case AccessType::TryGetById:
        return operationTryGetByIdOptimize;
    case AccessType::GetByIdDirect:
        return operationGetByIdDirectOptimize;
    case AccessType::GetByIdWithThis:
        return operationGetByIdWithThisOptimize;
    case AccessType::GetByValWithThis:
        return operationGetByValWithThisOptimize;
    case AccessType::HasPrivateName:
        return operationHasPrivateNameOptimize;
    case AccessType::HasPrivateBrand: 
        return operationHasPrivateBrandOptimize;
    case AccessType::GetPrivateName:
        return operationGetPrivateNameOptimize;
    case AccessType::GetPrivateNameById:
        return operationGetPrivateNameByIdOptimize;
    case AccessType::PutById:
        switch (unlinkedStubInfo.putKind) {
        case PutKind::NotDirect:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                return operationPutByIdStrictOptimize;
            else
                return operationPutByIdNonStrictOptimize;
        case PutKind::Direct:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                return operationPutByIdDirectStrictOptimize;
            else
                return operationPutByIdDirectNonStrictOptimize;
        case PutKind::DirectPrivateFieldDefine:
            return operationPutByIdDefinePrivateFieldStrictOptimize;
        case PutKind::DirectPrivateFieldSet:
            return operationPutByIdSetPrivateFieldStrictOptimize;
        }
        break;
    case AccessType::PutByVal:
        switch (unlinkedStubInfo.putKind) {
        case PutKind::NotDirect:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                return operationPutByValStrictOptimize;
            else
                return operationPutByValNonStrictOptimize;
        case PutKind::Direct:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                return operationDirectPutByValStrictOptimize;
            else
                return operationDirectPutByValNonStrictOptimize;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case AccessType::PutPrivateName:
        return unlinkedStubInfo.privateFieldPutKind.isDefine() ? operationPutByValDefinePrivateFieldOptimize : operationPutByValSetPrivateFieldOptimize;
    case AccessType::SetPrivateBrand:
        return operationSetPrivateBrandOptimize;
    case AccessType::CheckPrivateBrand:
        return operationCheckPrivateBrandOptimize;
    }
    return { };
}

void StructureStubInfo::initializeFromUnlinkedStructureStubInfo(const BaselineUnlinkedStructureStubInfo& unlinkedStubInfo)
{
    accessType = unlinkedStubInfo.accessType;
    doneLocation = unlinkedStubInfo.doneLocation;
    slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    callSiteIndex = CallSiteIndex(BytecodeIndex(unlinkedStubInfo.bytecodeIndex.offset()));
    codeOrigin = CodeOrigin(unlinkedStubInfo.bytecodeIndex);
    m_codePtr = slowPathStartLocation;
    propertyIsInt32 = unlinkedStubInfo.propertyIsInt32;
    tookSlowPath = unlinkedStubInfo.tookSlowPath;
    useDataIC = true;

    auto usedJSRs = RegisterSetBuilder::stubUnavailableRegisters();
    if (accessType == AccessType::GetById && unlinkedStubInfo.bytecodeIndex.checkpoint()) {
        // For iterator_next, we can't clobber the "dontClobberJSR" register either.
        usedJSRs.add(BaselineJITRegisters::GetById::FastPath::dontClobberJSR, IgnoreVectors);
    }
    usedRegisters = usedJSRs.buildScalarRegisterSet();

    m_slowOperation = slowOperationFromUnlinkedStructureStubInfo(unlinkedStubInfo);

    switch (accessType) {
    case AccessType::DeleteByVal:
        hasConstantIdentifier = false;
        m_baseGPR = BaselineJITRegisters::DelByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::DelByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::DelByVal::FastPath::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::DelByVal::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::DelByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::DelByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::DelByVal::FastPath::resultJSR.tagGPR();
#endif
        break;
    case AccessType::DeleteByID:
        hasConstantIdentifier = true;
        m_baseGPR = BaselineJITRegisters::DelById::baseJSR.payloadGPR();
        m_extraGPR = InvalidGPRReg;
        m_valueGPR = BaselineJITRegisters::DelById::FastPath::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::DelById::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::DelById::baseJSR.tagGPR();
        m_extraTagGPR = InvalidGPRReg;
        m_valueTagGPR = BaselineJITRegisters::DelById::FastPath::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        hasConstantIdentifier = false;
        m_baseGPR = BaselineJITRegisters::GetByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByVal::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByVal::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::GetByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::GetByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetByVal::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InstanceOf:
        hasConstantIdentifier = false;
        prototypeIsKnownObject = false;
        m_baseGPR = BaselineJITRegisters::Instanceof::valueJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::Instanceof::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::Instanceof::protoJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::Instanceof::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::Instanceof::valueJSR.tagGPR();
        m_valueTagGPR = InvalidGPRReg;
        m_extraTagGPR = BaselineJITRegisters::Instanceof::protoJSR.tagGPR();
#endif
        break;
    case AccessType::InByVal:
    case AccessType::HasPrivateName: 
    case AccessType::HasPrivateBrand: 
        hasConstantIdentifier = false;
        m_baseGPR = BaselineJITRegisters::InByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::InByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::InByVal::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::InByVal::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::InByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::InByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::InByVal::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InById:
        hasConstantIdentifier = true;
        m_extraGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::InById::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::InById::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::InById::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_extraTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::InById::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::InById::resultJSR.tagGPR();
#endif
        break;
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::GetById:
    case AccessType::GetPrivateNameById:
        hasConstantIdentifier = true;
        m_extraGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::GetById::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetById::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetById::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_extraTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::GetById::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetById::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByIdWithThis:
        hasConstantIdentifier = true;
        m_baseGPR = BaselineJITRegisters::GetByIdWithThis::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByIdWithThis::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByIdWithThis::thisJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByIdWithThis::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::GetByIdWithThis::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetByIdWithThis::resultJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::GetByIdWithThis::thisJSR.tagGPR();
#endif
        break;
    case AccessType::GetByValWithThis:
        hasConstantIdentifier = false;
#if USE(JSVALUE64)
        m_baseGPR = BaselineJITRegisters::GetByValWithThis::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByValWithThis::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByValWithThis::thisJSR.payloadGPR();
        m_extra2GPR = BaselineJITRegisters::GetByValWithThis::propertyJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByValWithThis::FastPath::stubInfoGPR;
#else
        // Registers are exhausted, we cannot have this IC on 32bit.
        RELEASE_ASSERT_NOT_REACHED();
#endif
        break;
    case AccessType::PutById:
        hasConstantIdentifier = true;
        m_extraGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::PutById::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::PutById::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PutById::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_extraTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::PutById::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::PutById::valueJSR.tagGPR();
#endif
        break;
    case AccessType::PutByVal:
    case AccessType::PutPrivateName:
        hasConstantIdentifier = false;
        m_baseGPR = BaselineJITRegisters::PutByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::PutByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::PutByVal::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PutByVal::stubInfoGPR;
        if (accessType == AccessType::PutByVal)
            m_arrayProfileGPR = BaselineJITRegisters::PutByVal::profileGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::PutByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::PutByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::PutByVal::valueJSR.tagGPR();
#endif
        break;
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        hasConstantIdentifier = false;
        m_valueGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::PrivateBrand::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::PrivateBrand::brandJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PrivateBrand::FastPath::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_valueTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::PrivateBrand::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::PrivateBrand::brandJSR.tagGPR();
#endif
        break;
    }
}

#if ENABLE(DFG_JIT)
void StructureStubInfo::initializeFromDFGUnlinkedStructureStubInfo(const DFG::UnlinkedStructureStubInfo& unlinkedStubInfo)
{
    accessType = unlinkedStubInfo.accessType;
    doneLocation = unlinkedStubInfo.doneLocation;
    slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    callSiteIndex = unlinkedStubInfo.callSiteIndex;
    codeOrigin = unlinkedStubInfo.codeOrigin;
    m_codePtr = slowPathStartLocation;

    propertyIsInt32 = unlinkedStubInfo.propertyIsInt32;
    propertyIsSymbol = unlinkedStubInfo.propertyIsSymbol;
    propertyIsString = unlinkedStubInfo.propertyIsString;
    prototypeIsKnownObject = unlinkedStubInfo.prototypeIsKnownObject;
    hasConstantIdentifier = unlinkedStubInfo.hasConstantIdentifier;
    tookSlowPath = unlinkedStubInfo.tookSlowPath;
    useDataIC = true;

    usedRegisters = unlinkedStubInfo.usedRegisters;

    m_baseGPR = unlinkedStubInfo.m_baseGPR;
    m_extraGPR = unlinkedStubInfo.m_extraGPR;
    m_extra2GPR = unlinkedStubInfo.m_extra2GPR;
    m_valueGPR = unlinkedStubInfo.m_valueGPR;
    m_stubInfoGPR = unlinkedStubInfo.m_stubInfoGPR;

    m_slowOperation = slowOperationFromUnlinkedStructureStubInfo(unlinkedStubInfo);
}
#endif


#if ASSERT_ENABLED
void StructureStubInfo::checkConsistency()
{
    switch (accessType) {
    case AccessType::GetByIdWithThis:
        // We currently use a union for both "thisGPR" and "propertyGPR". If this were
        // not the case, we'd need to take one of them out of the union.
        RELEASE_ASSERT(hasConstantIdentifier);
        break;
    case AccessType::GetByValWithThis:
    default:
        break;
    }
}
#endif // ASSERT_ENABLED

#endif // ENABLE(JIT)

} // namespace JSC
