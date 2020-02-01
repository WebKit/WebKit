/*
 * Copyright (C) 2008-2020 Apple Inc. All rights reserved.
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
#include "JSObject.h"
#include "JSCInlines.h"
#include "PolymorphicAccess.h"
#include "Repatch.h"

namespace JSC {

#if ENABLE(JIT)

namespace StructureStubInfoInternal {
static constexpr bool verbose = false;
}

StructureStubInfo::StructureStubInfo(AccessType accessType)
    : accessType(accessType)
    , m_cacheType(CacheType::Unset)
    , countdown(1) // For a totally clear stub, we'll patch it after the first execution.
    , repatchCount(0)
    , numberOfCoolDowns(0)
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
}

StructureStubInfo::~StructureStubInfo()
{
}

void StructureStubInfo::initGetByIdSelf(CodeBlock* codeBlock, Structure* baseObjectStructure, PropertyOffset offset, CacheableIdentifier identifier)
{
    ASSERT(hasConstantIdentifier);
    setCacheType(CacheType::GetByIdSelf);
    m_getByIdSelfIdentifier = identifier;
    codeBlock->vm().heap.writeBarrier(codeBlock);
    
    u.byIdSelf.baseObjectStructure.set(
        codeBlock->vm(), codeBlock, baseObjectStructure);
    u.byIdSelf.offset = offset;
}

void StructureStubInfo::initArrayLength()
{
    setCacheType(CacheType::ArrayLength);
}

void StructureStubInfo::initStringLength()
{
    setCacheType(CacheType::StringLength);
}

void StructureStubInfo::initPutByIdReplace(CodeBlock* codeBlock, Structure* baseObjectStructure, PropertyOffset offset)
{
    setCacheType(CacheType::PutByIdReplace);
    
    u.byIdSelf.baseObjectStructure.set(
        codeBlock->vm(), codeBlock, baseObjectStructure);
    u.byIdSelf.offset = offset;
}

void StructureStubInfo::initInByIdSelf(CodeBlock* codeBlock, Structure* baseObjectStructure, PropertyOffset offset)
{
    setCacheType(CacheType::InByIdSelf);

    u.byIdSelf.baseObjectStructure.set(
        codeBlock->vm(), codeBlock, baseObjectStructure);
    u.byIdSelf.offset = offset;
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
    const GCSafeConcurrentJSLocker& locker, CodeBlock* codeBlock, CacheableIdentifier ident, std::unique_ptr<AccessCase> accessCase)
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
            result = u.stub->addCase(locker, vm, codeBlock, *this, WTFMove(accessCase));
            
            if (StructureStubInfoInternal::verbose)
                dataLog("Had stub, result: ", result, "\n");

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                bufferedStructures.clear();
                return result;
            }
        } else {
            std::unique_ptr<PolymorphicAccess> access = makeUnique<PolymorphicAccess>();
            
            Vector<std::unique_ptr<AccessCase>, 2> accessCases;
            
            std::unique_ptr<AccessCase> previousCase = AccessCase::fromStructureStubInfo(vm, codeBlock, ident, *this);
            if (previousCase)
                accessCases.append(WTFMove(previousCase));
            
            accessCases.append(WTFMove(accessCase));
            
            result = access->addCases(locker, vm, codeBlock, *this, WTFMove(accessCases));
            
            if (StructureStubInfoInternal::verbose)
                dataLog("Created stub, result: ", result, "\n");

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                bufferedStructures.clear();
                return result;
            }
            
            setCacheType(CacheType::Stub);
            u.stub = access.release();
        }
        
        ASSERT(m_cacheType == CacheType::Stub);
        RELEASE_ASSERT(!result.generatedSomeCode());
        
        // If we didn't buffer any cases then bail. If this made no changes then we'll just try again
        // subject to cool-down.
        if (!result.buffered()) {
            if (StructureStubInfoInternal::verbose)
                dataLog("Didn't buffer anything, bailing.\n");
            bufferedStructures.clear();
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
        bufferedStructures.clear();
        
        result = u.stub->regenerate(locker, vm, codeBlock, *this);
        
        if (StructureStubInfoInternal::verbose)
            dataLog("Regeneration result: ", result, "\n");
        
        RELEASE_ASSERT(!result.buffered());
        
        if (!result.generatedSomeCode())
            return result;
        
        // If we generated some code then we don't want to attempt to repatch in the future until we
        // gather enough cases.
        bufferingCountdown = Options::repatchBufferingCountdown();
        return result;
    })();
    vm.heap.writeBarrier(codeBlock);
    return result;
}

void StructureStubInfo::reset(CodeBlock* codeBlock)
{
    bufferedStructures.clear();

    if (m_cacheType == CacheType::Unset)
        return;

    if (Options::verboseOSR()) {
        // This can be called from GC destructor calls, so we don't try to do a full dump
        // of the CodeBlock.
        dataLog("Clearing structure cache (kind ", static_cast<int>(accessType), ") in ", RawPointer(codeBlock), ".\n");
    }

    switch (accessType) {
    case AccessType::TryGetById:
        resetGetBy(codeBlock, *this, GetByKind::Try);
        break;
    case AccessType::GetById:
        resetGetBy(codeBlock, *this, GetByKind::Normal);
        break;
    case AccessType::GetByIdWithThis:
        resetGetBy(codeBlock, *this, GetByKind::WithThis);
        break;
    case AccessType::GetByIdDirect:
        resetGetBy(codeBlock, *this, GetByKind::Direct);
        break;
    case AccessType::GetByVal:
        resetGetBy(codeBlock, *this, GetByKind::NormalByVal);
        break;
    case AccessType::Put:
        resetPutByID(codeBlock, *this);
        break;
    case AccessType::In:
        resetInByID(codeBlock, *this);
        break;
    case AccessType::InstanceOf:
        resetInstanceOf(*this);
        break;
    }
    
    deref();
    setCacheType(CacheType::Unset);
}

void StructureStubInfo::visitAggregate(SlotVisitor& visitor)
{
    switch (m_cacheType) {
    case CacheType::Unset:
    case CacheType::ArrayLength:
    case CacheType::StringLength:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
        return;
    case CacheType::GetByIdSelf:
        m_getByIdSelfIdentifier.visitAggregate(visitor);
        return;
    case CacheType::Stub:
        u.stub->visitAggregate(visitor);
        return;
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return;
}

void StructureStubInfo::visitWeakReferences(CodeBlock* codeBlock)
{
    VM& vm = codeBlock->vm();
    
    bufferedStructures.removeIf(
        [&] (auto& pair) -> bool {
            Structure* structure = pair.first;
            return !vm.heap.isMarked(structure);
        });

    switch (m_cacheType) {
    case CacheType::GetByIdSelf:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
        if (vm.heap.isMarked(u.byIdSelf.baseObjectStructure.get()))
            return;
        break;
    case CacheType::Stub:
        if (u.stub->visitWeak(vm))
            return;
        break;
    default:
        return;
    }

    reset(codeBlock);
    resetByGC = true;
}

bool StructureStubInfo::propagateTransitions(SlotVisitor& visitor)
{
    switch (m_cacheType) {
    case CacheType::Unset:
    case CacheType::ArrayLength:
    case CacheType::StringLength:
        return true;
    case CacheType::GetByIdSelf:
    case CacheType::PutByIdReplace:
    case CacheType::InByIdSelf:
        return u.byIdSelf.baseObjectStructure->markIfCheap(visitor);
    case CacheType::Stub:
        return u.stub->propagateTransitions(visitor);
    }
    
    RELEASE_ASSERT_NOT_REACHED();
    return true;
}

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

void StructureStubInfo::setCacheType(CacheType newCacheType)
{
    if (m_cacheType == CacheType::GetByIdSelf)
        m_getByIdSelfIdentifier = nullptr;
    m_cacheType = newCacheType;
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
