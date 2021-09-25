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

StructureStubInfo::StructureStubInfo(AccessType accessType, CodeOrigin codeOrigin)
    : codeOrigin(codeOrigin)
    , accessType(accessType)
    , bufferingCountdown(Options::repatchBufferingCountdown())
    , resetByGC(false)
    , tookSlowPath(false)
    , everConsidered(false)
    , prototypeIsKnownObject(false)
    , sawNonCell(false)
    , hasConstantIdentifier(true)
    , propertyIsString(false)
    , propertyIsInt32(false)
    , propertyIsSymbol(false)
{
    regs.thisGPR = InvalidGPRReg;
}

StructureStubInfo::~StructureStubInfo()
{
}

void StructureStubInfo::initGetByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier);
    setCacheType(locker, CacheType::GetByIdSelf);
    m_identifier = identifier;
    m_inlineAccessBaseStructure = inlineAccessBaseStructure->id();
    codeBlock->vm().heap.writeBarrier(codeBlock);
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
    m_inlineAccessBaseStructure = inlineAccessBaseStructure->id();
    codeBlock->vm().heap.writeBarrier(codeBlock);
    byIdSelfOffset = offset;
}

void StructureStubInfo::initInByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::InByIdSelf);
    m_identifier = identifier;
    m_inlineAccessBaseStructure = inlineAccessBaseStructure->id();
    codeBlock->vm().heap.writeBarrier(codeBlock);
    byIdSelfOffset = offset;
}

void StructureStubInfo::deref()
{
    switch (m_cacheType) {
    case CacheType::Stub:
        delete u.stub;
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
        u.stub->aboutToDie();
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
            result = u.stub->addCase(locker, vm, codeBlock, *this, accessCase.releaseNonNull());
            
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
            u.stub = access.release();
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
        
        result = u.stub->regenerate(locker, vm, globalObject, codeBlock, ecmaMode, *this);
        
        if (StructureStubInfoInternal::verbose)
            dataLog("Regeneration result: ", result, "\n");
        
        RELEASE_ASSERT(!result.buffered());
        
        if (!result.generatedSomeCode())
            return result;

        // When we first transition to becoming a Stub, we might still be running the inline
        // access code. That's because when we first transition to becoming a Stub, we may
        // be buffered, and we have not yet generated any code. Once the Stub finally generates
        // code, we're no longer running the inline access code, so we can then clear out
        // m_inlineAccessBaseStructure. The reason we don't clear m_inlineAccessBaseStructure while
        // we're buffered is because we rely on it to reset during GC if m_inlineAccessBaseStructure
        // is collected.
        m_identifier = nullptr;
        m_inlineAccessBaseStructure = 0;
        
        // If we generated some code then we don't want to attempt to repatch in the future until we
        // gather enough cases.
        bufferingCountdown = Options::repatchBufferingCountdown();
        return result;
    })();
    vm.heap.writeBarrier(codeBlock);
    return result;
}

void StructureStubInfo::reset(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    clearBufferedStructures();
    m_identifier = nullptr;
    m_inlineAccessBaseStructure = 0;

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
        u.stub->visitAggregate(visitor);
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
        isValid &= u.stub->visitWeak(vm);

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
        u.stub->propagateTransitions(visitor);
}

template void StructureStubInfo::propagateTransitions(AbstractSlotVisitor&);
template void StructureStubInfo::propagateTransitions(SlotVisitor&);

StubInfoSummary StructureStubInfo::summary(VM& vm) const
{
    StubInfoSummary takesSlowPath = StubInfoSummary::TakesSlowPath;
    StubInfoSummary simple = StubInfoSummary::Simple;
    if (m_cacheType == CacheType::Stub) {
        PolymorphicAccess* list = u.stub;
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
    return u.stub->containsPC(pc);
}

ALWAYS_INLINE void StructureStubInfo::setCacheType(const ConcurrentJSLockerBase&, CacheType newCacheType)
{
    m_cacheType = newCacheType;
}

void StructureStubInfo::initializeFromUnlinkedStructureStubInfo(CodeBlock*, UnlinkedStructureStubInfo& unlinkedStubInfo)
{
#if USE(JSVALUE64)
    accessType = unlinkedStubInfo.accessType;
    start = unlinkedStubInfo.start;
    doneLocation = unlinkedStubInfo.doneLocation;
    slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    callSiteIndex = CallSiteIndex(BytecodeIndex(unlinkedStubInfo.bytecodeIndex.offset()));
    codeOrigin = CodeOrigin(unlinkedStubInfo.bytecodeIndex);
    m_codePtr = slowPathStartLocation;

    usedRegisters = RegisterSet::stubUnavailableRegisters();
    if (accessType == AccessType::GetById && unlinkedStubInfo.bytecodeIndex.checkpoint()) {
        // For iterator_next, we can't clobber the "dontClobberRegister" register either.
        usedRegisters.add(BaselineGetByIdRegisters::dontClobberRegister);
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
        baseGPR = BaselineDelByValRegisters::base;
        regs.propertyGPR = BaselineDelByValRegisters::property;
        valueGPR = BaselineDelByValRegisters::result;
        m_stubInfoGPR = BaselineDelByValRegisters::stubInfo;
        break;
    case AccessType::DeleteByID:
        hasConstantIdentifier = true;
        baseGPR = BaselineDelByIdRegisters::base;
        regs.propertyGPR = InvalidGPRReg;
        valueGPR = BaselineDelByIdRegisters::result;
        m_stubInfoGPR = BaselineDelByIdRegisters::stubInfo;
        break;
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        hasConstantIdentifier = false;
        baseGPR = BaselineGetByValRegisters::base;
        regs.propertyGPR = BaselineGetByValRegisters::property;
        valueGPR = BaselineGetByValRegisters::result;
        m_stubInfoGPR = BaselineGetByValRegisters::stubInfo;
        break;
    case AccessType::InstanceOf:
        hasConstantIdentifier = false;
        prototypeIsKnownObject = false;
        baseGPR = BaselineInstanceofRegisters::value;
        valueGPR = BaselineInstanceofRegisters::result;
        regs.prototypeGPR = BaselineInstanceofRegisters::proto;
        m_stubInfoGPR = BaselineInstanceofRegisters::stubInfo;
        break;
    case AccessType::InByVal:
    case AccessType::HasPrivateName: 
    case AccessType::HasPrivateBrand: 
        hasConstantIdentifier = false;
        baseGPR = BaselineInByValRegisters::base;
        regs.propertyGPR = BaselineInByValRegisters::property;
        valueGPR = BaselineInByValRegisters::result;
        m_stubInfoGPR = BaselineInByValRegisters::stubInfo;
        break;
    case AccessType::InById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselineInByIdRegisters::base;
        valueGPR = BaselineInByIdRegisters::result;
        m_stubInfoGPR = BaselineInByIdRegisters::stubInfo;
        break;
    case AccessType::TryGetById:
    case AccessType::GetByIdDirect:
    case AccessType::GetById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselineGetByIdRegisters::base;
        valueGPR = BaselineGetByIdRegisters::result;
        m_stubInfoGPR = BaselineGetByIdRegisters::stubInfo;
        break;
    case AccessType::GetByIdWithThis:
        hasConstantIdentifier = true;
        baseGPR = BaselineGetByIdWithThisRegisters::base;
        valueGPR = BaselineGetByIdWithThisRegisters::result;
        regs.thisGPR = BaselineGetByIdWithThisRegisters::thisValue;
        m_stubInfoGPR = BaselineGetByIdWithThisRegisters::stubInfo;
        break;
    case AccessType::PutById:
        hasConstantIdentifier = true;
        regs.thisGPR = InvalidGPRReg;
        baseGPR = BaselinePutByIdRegisters::base;
        valueGPR = BaselinePutByIdRegisters::value;
        m_stubInfoGPR = BaselinePutByIdRegisters::stubInfo;
        break;
    case AccessType::PutByVal:
    case AccessType::PutPrivateName:
        hasConstantIdentifier = false;
        baseGPR = BaselinePutByValRegisters::base;
        regs.propertyGPR = BaselinePutByValRegisters::property;
        valueGPR = BaselinePutByValRegisters::value;
        m_stubInfoGPR = BaselinePutByValRegisters::stubInfo;
        if (accessType == AccessType::PutByVal)
            m_arrayProfileGPR = BaselinePutByValRegisters::profile;
        break;
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        hasConstantIdentifier = false;
        valueGPR = InvalidGPRReg;
        baseGPR = BaselinePrivateBrandRegisters::base;
        regs.brandGPR = BaselinePrivateBrandRegisters::brand;
        m_stubInfoGPR = BaselinePrivateBrandRegisters::stubInfo;
        break;
    }
#else
    UNUSED_PARAM(unlinkedStubInfo);
    ASSERT_NOT_REACHED();
#endif
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
