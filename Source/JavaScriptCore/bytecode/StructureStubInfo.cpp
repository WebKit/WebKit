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

#include "CacheableIdentifierInlines.h"
#include "JITInlineCacheGenerator.h"
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
    case AccessType::GetPrivateName:
        resetGetBy(codeBlock, *this, GetByKind::PrivateName);
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
    if (Structure* structure = inlineAccessBaseStructure(vm))
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
    if (Structure* structure = inlineAccessBaseStructure(visitor.vm()))
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

void StructureStubInfo::initializeFromUnlinkedStructureStubInfo(CodeBlock*, UnlinkedStructureStubInfo& unlinkedStubInfo)
{
    accessType = unlinkedStubInfo.accessType;
    start = unlinkedStubInfo.start;
    doneLocation = unlinkedStubInfo.doneLocation;
    slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    callSiteIndex = CallSiteIndex(BytecodeIndex(unlinkedStubInfo.bytecodeIndex.offset()));
    codeOrigin = CodeOrigin(unlinkedStubInfo.bytecodeIndex);
    m_codePtr = slowPathStartLocation;
    propertyIsInt32 = unlinkedStubInfo.propertyIsInt32;

    usedRegisters = RegisterSet::stubUnavailableRegisters();
    if (accessType == AccessType::GetById && unlinkedStubInfo.bytecodeIndex.checkpoint()) {
        // For iterator_next, we can't clobber the "dontClobberJSR" register either.
        usedRegisters.set(BaselineGetByIdRegisters::dontClobberJSR);
    }

    switch (accessType) {
    case AccessType::DeleteByVal:
        m_slowOperation = operationDeleteByValOptimize;
        break;
    case AccessType::DeleteByID:
        m_slowOperation = operationDeleteByIdOptimize;
        break;
    case AccessType::GetByVal:
        m_slowOperation = operationGetByValOptimize;
        break;
    case AccessType::InstanceOf:
        m_slowOperation = operationInstanceOfOptimize;
        break;
    case AccessType::InByVal:
        m_slowOperation = operationInByValOptimize;
        break;
    case AccessType::InById:
        m_slowOperation = operationInByIdOptimize;
        break;
    case AccessType::GetById:
        m_slowOperation = operationGetByIdOptimize;
        break;
    case AccessType::TryGetById:
        m_slowOperation = operationTryGetByIdOptimize;
        break;
    case AccessType::GetByIdDirect:
        m_slowOperation = operationGetByIdDirectOptimize;
        break;
    case AccessType::GetByIdWithThis:
        m_slowOperation = operationGetByIdWithThisOptimize;
        break;
    case AccessType::HasPrivateName:
        m_slowOperation = operationHasPrivateNameOptimize;
        break;
    case AccessType::HasPrivateBrand: 
        m_slowOperation = operationHasPrivateBrandOptimize;
        break;
    case AccessType::GetPrivateName:
        m_slowOperation = operationGetPrivateNameOptimize;
        break;
    case AccessType::PutById:
        switch (unlinkedStubInfo.putKind) {
        case PutKind::NotDirect:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                m_slowOperation = operationPutByIdStrictOptimize;
            else
                m_slowOperation = operationPutByIdNonStrictOptimize;
            break;
        case PutKind::Direct:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                m_slowOperation = operationPutByIdDirectStrictOptimize;
            else
                m_slowOperation = operationPutByIdDirectNonStrictOptimize;
            break;
        case PutKind::DirectPrivateFieldDefine:
            m_slowOperation = operationPutByIdDefinePrivateFieldStrictOptimize;
            break;
        case PutKind::DirectPrivateFieldSet:
            m_slowOperation = operationPutByIdSetPrivateFieldStrictOptimize;
            break;
        }
        break;
    case AccessType::PutByVal:
        switch (unlinkedStubInfo.putKind) {
        case PutKind::NotDirect:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                m_slowOperation = operationPutByValStrictOptimize;
            else
                m_slowOperation = operationPutByValNonStrictOptimize;
            break;
        case PutKind::Direct:
            if (unlinkedStubInfo.ecmaMode.isStrict())
                m_slowOperation = operationDirectPutByValStrictOptimize;
            else
                m_slowOperation = operationDirectPutByValNonStrictOptimize;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        break;
    case AccessType::PutPrivateName:
        m_slowOperation = unlinkedStubInfo.privateFieldPutKind.isDefine() ? operationPutByValDefinePrivateFieldOptimize : operationPutByValSetPrivateFieldOptimize;
        break;
    case AccessType::SetPrivateBrand:
        m_slowOperation = operationSetPrivateBrandOptimize;
        break;
    case AccessType::CheckPrivateBrand:
        m_slowOperation = operationCheckPrivateBrandOptimize;
        break;
    }

    switch (accessType) {
    case AccessType::DeleteByVal:
        hasConstantIdentifier = false;
        baseGPR = BaselineDelByValRegisters::baseJSR.payloadGPR();
        regs.propertyGPR = BaselineDelByValRegisters::propertyJSR.payloadGPR();
        valueGPR = BaselineDelByValRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineDelByValRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineDelByValRegisters::baseJSR.tagGPR();
        v.propertyTagGPR = BaselineDelByValRegisters::propertyJSR.tagGPR();
        valueTagGPR = BaselineDelByValRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::DeleteByID:
        hasConstantIdentifier = true;
        baseGPR = BaselineDelByIdRegisters::baseJSR.payloadGPR();
        regs.propertyGPR = InvalidGPRReg;
        valueGPR = BaselineDelByIdRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineDelByIdRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineDelByIdRegisters::baseJSR.tagGPR();
        v.propertyTagGPR = InvalidGPRReg;
        valueTagGPR = BaselineDelByIdRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        hasConstantIdentifier = false;
        baseGPR = BaselineGetByValRegisters::baseJSR.payloadGPR();
        regs.propertyGPR = BaselineGetByValRegisters::propertyJSR.payloadGPR();
        valueGPR = BaselineGetByValRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineGetByValRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineGetByValRegisters::baseJSR.tagGPR();
        v.propertyTagGPR = BaselineGetByValRegisters::propertyJSR.tagGPR();
        valueTagGPR = BaselineGetByValRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InstanceOf:
        hasConstantIdentifier = false;
        prototypeIsKnownObject = false;
        baseGPR = BaselineInstanceofRegisters::valueJSR.payloadGPR();
        valueGPR = BaselineInstanceofRegisters::resultJSR.payloadGPR();
        regs.prototypeGPR = BaselineInstanceofRegisters::protoJSR.payloadGPR();
        m_stubInfoGPR = BaselineInstanceofRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineInstanceofRegisters::valueJSR.tagGPR();
        valueTagGPR = InvalidGPRReg;
        v.propertyTagGPR = BaselineInstanceofRegisters::protoJSR.tagGPR();
#endif
        break;
    case AccessType::InByVal:
    case AccessType::HasPrivateName: 
    case AccessType::HasPrivateBrand: 
        hasConstantIdentifier = false;
        baseGPR = BaselineInByValRegisters::baseJSR.payloadGPR();
        regs.propertyGPR = BaselineInByValRegisters::propertyJSR.payloadGPR();
        valueGPR = BaselineInByValRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineInByValRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineInByValRegisters::baseJSR.tagGPR();
        v.propertyTagGPR = BaselineInByValRegisters::propertyJSR.tagGPR();
        valueTagGPR = BaselineInByValRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselineInByIdRegisters::baseJSR.payloadGPR();
        valueGPR = BaselineInByIdRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineInByIdRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        v.thisTagGPR = InvalidGPRReg;
        baseTagGPR = BaselineInByIdRegisters::baseJSR.tagGPR();
        valueTagGPR = BaselineInByIdRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::GetById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselineGetByIdRegisters::baseJSR.payloadGPR();
        valueGPR = BaselineGetByIdRegisters::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineGetByIdRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        v.thisTagGPR = InvalidGPRReg;
        baseTagGPR = BaselineGetByIdRegisters::baseJSR.tagGPR();
        valueTagGPR = BaselineGetByIdRegisters::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByIdWithThis:
        hasConstantIdentifier = true;
        baseGPR = BaselineGetByIdWithThisRegisters::baseJSR.payloadGPR();
        valueGPR = BaselineGetByIdWithThisRegisters::resultJSR.payloadGPR();
        regs.thisGPR = BaselineGetByIdWithThisRegisters::thisJSR.payloadGPR();
        m_stubInfoGPR = BaselineGetByIdWithThisRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselineGetByIdWithThisRegisters::baseJSR.tagGPR();
        valueTagGPR = BaselineGetByIdWithThisRegisters::resultJSR.tagGPR();
        v.thisTagGPR = BaselineGetByIdWithThisRegisters::thisJSR.tagGPR();
#endif
        break;
    case AccessType::PutById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselinePutByIdRegisters::baseJSR.payloadGPR();
        valueGPR = BaselinePutByIdRegisters::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselinePutByIdRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        v.thisTagGPR = InvalidGPRReg;
        baseTagGPR = BaselinePutByIdRegisters::baseJSR.tagGPR();
        valueTagGPR = BaselinePutByIdRegisters::valueJSR.tagGPR();
#endif
        break;
    case AccessType::PutByVal:
    case AccessType::PutPrivateName:
        hasConstantIdentifier = false;
        baseGPR = BaselinePutByValRegisters::baseJSR.payloadGPR();
        regs.propertyGPR = BaselinePutByValRegisters::propertyJSR.payloadGPR();
        valueGPR = BaselinePutByValRegisters::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselinePutByValRegisters::stubInfoGPR;
        if (accessType == AccessType::PutByVal)
            m_arrayProfileGPR = BaselinePutByValRegisters::profileGPR;
#if USE(JSVALUE32_64)
        baseTagGPR = BaselinePutByValRegisters::baseJSR.tagGPR();
        v.propertyTagGPR = BaselinePutByValRegisters::propertyJSR.tagGPR();
        valueTagGPR = BaselinePutByValRegisters::valueJSR.tagGPR();
#endif
        break;
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        hasConstantIdentifier = false;
        valueGPR = InvalidGPRReg;
        baseGPR = BaselinePrivateBrandRegisters::baseJSR.payloadGPR();
        regs.brandGPR = BaselinePrivateBrandRegisters::brandJSR.payloadGPR();
        m_stubInfoGPR = BaselinePrivateBrandRegisters::stubInfoGPR;
#if USE(JSVALUE32_64)
        valueTagGPR = InvalidGPRReg;
        baseTagGPR = BaselinePrivateBrandRegisters::baseJSR.tagGPR();
        v.brandTagGPR = BaselinePrivateBrandRegisters::brandJSR.tagGPR();
#endif
        break;
    }
}

#if ASSERT_ENABLED
void StructureStubInfo::checkConsistency()
{
    if (thisValueIsInThisGPR()) {
        // We currently use a union for both "thisGPR" and "propertyGPR". If this were
        // not the case, we'd need to take one of them out of the union.
        RELEASE_ASSERT(hasConstantIdentifier);
    }
}
#endif // ASSERT_ENABLED

#endif // ENABLE(JIT)

} // namespace JSC
