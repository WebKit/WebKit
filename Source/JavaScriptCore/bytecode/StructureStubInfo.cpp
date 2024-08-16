/*
 * Copyright (C) 2008-2024 Apple Inc. All rights reserved.
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
#include "InlineCacheCompiler.h"
#include "Repatch.h"

namespace JSC {

#if ENABLE(JIT)

namespace StructureStubInfoInternal {
static constexpr bool verbose = false;
}

StructureStubInfo::~StructureStubInfo() = default;

void StructureStubInfo::initGetByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::GetByIdSelf);
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

void StructureStubInfo::initPutByIdReplace(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::PutByIdReplace);
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void StructureStubInfo::initInByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::InByIdSelf);
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void StructureStubInfo::deref()
{
    m_stub.reset();
}

void StructureStubInfo::aboutToDie()
{
    if (m_inlinedHandler)
        m_inlinedHandler->aboutToDie();

    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            cursor->aboutToDie();
            cursor = cursor->next();
        }
    }
}

AccessGenerationResult StructureStubInfo::addAccessCase(const GCSafeConcurrentJSLocker& locker, JSGlobalObject* globalObject, CodeBlock* codeBlock, ECMAMode ecmaMode, CacheableIdentifier ident, RefPtr<AccessCase> accessCase)
{
    checkConsistency();

    VM& vm = codeBlock->vm();
    ASSERT(vm.heap.isDeferred());

    if (!accessCase)
        return AccessGenerationResult::GaveUp;

    AccessGenerationResult result = ([&](Ref<AccessCase>&& accessCase) -> AccessGenerationResult {
        dataLogLnIf(StructureStubInfoInternal::verbose, "Adding access case: ", accessCase);

        AccessGenerationResult result;

        if (useHandlerIC()) {
            if (!m_stub) {
                setCacheType(locker, CacheType::Stub);
                m_stub = makeUnique<PolymorphicAccess>();
            }
        }

        if (m_stub) {
            result = m_stub->addCases(locker, vm, codeBlock, *this, nullptr, accessCase);
            dataLogLnIf(StructureStubInfoInternal::verbose, "Had stub, result: ", result);

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                clearBufferedStructures();
                return result;
            }
        } else {
            std::unique_ptr<PolymorphicAccess> access = makeUnique<PolymorphicAccess>();
            result = access->addCases(locker, vm, codeBlock, *this, AccessCase::fromStructureStubInfo(vm, codeBlock, ident, *this), accessCase);

            dataLogLnIf(StructureStubInfoInternal::verbose, "Created stub, result: ", result);

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
            dataLogLnIf(StructureStubInfoInternal::verbose, "Didn't buffer anything, bailing.");
            clearBufferedStructures();
            return result;
        }

        if (useHandlerIC()) {
            InlineCacheCompiler compiler(codeBlock->jitType(), vm, globalObject, ecmaMode, *this);
            return compiler.compileHandler(locker, *m_stub, codeBlock, WTFMove(accessCase));
        }

        // The buffering countdown tells us if we should be repatching now.
        if (bufferingCountdown) {
            dataLogLnIf(StructureStubInfoInternal::verbose, "Countdown is too high: ", bufferingCountdown, ".");
            return result;
        }

        // Forget the buffered structures so that all future attempts to cache get fully handled by the
        // PolymorphicAccess.
        clearBufferedStructures();

        InlineCacheCompiler compiler(codeBlock->jitType(), vm, globalObject, ecmaMode, *this);
        result = compiler.compile(locker, *m_stub, codeBlock);

        dataLogLnIf(StructureStubInfoInternal::verbose, "Regeneration result: ", result);

        RELEASE_ASSERT(!result.buffered());
        
        if (!result.generatedSomeCode())
            return result;

        // If we are using DataIC, we will continue using inlined code for the first case.
        if (!useDataIC) {
            // When we first transition to becoming a Stub, we might still be running the inline
            // access code. That's because when we first transition to becoming a Stub, we may
            // be buffered, and we have not yet generated any code. Once the Stub finally generates
            // code, we're no longer running the inline access code, so we can then clear out
            // m_inlineAccessBaseStructureID. The reason we don't clear m_inlineAccessBaseStructureID while
            // we're buffered is because we rely on it to reset during GC if m_inlineAccessBaseStructureID
            // is collected.
            m_inlineAccessBaseStructureID.clear();
        }

        // If we generated some code then we don't want to attempt to repatch in the future until we
        // gather enough cases.
        bufferingCountdown = Options::repatchBufferingCountdown();
        return result;
    })(accessCase.releaseNonNull());
    if (result.generatedSomeCode()) {
        if (useHandlerIC())
            prependHandler(codeBlock, Ref { *result.handler() }, result.generatedMegamorphicCode());
        else
            rewireStubAsJumpInAccess(codeBlock, Ref { *result.handler() });
    }

    vm.writeBarrier(codeBlock);
    return result;
}

void StructureStubInfo::reset(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    clearBufferedStructures();
    m_inlineAccessBaseStructureID.clear();
    if (m_inlinedHandler)
        clearInlinedHandler(codeBlock);

    if (m_cacheType == CacheType::Unset)
        return;

    // This can be called from GC destructor calls, so we don't try to do a full dump
    // of the CodeBlock.
    dataLogLnIf(Options::verboseOSR(), "Clearing structure cache (kind ", static_cast<int>(accessType), ") in ", RawPointer(codeBlock), ".");

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
    case AccessType::PutByIdStrict:
        resetPutBy(codeBlock, *this, PutByKind::ByIdStrict);
        break;
    case AccessType::PutByIdSloppy:
        resetPutBy(codeBlock, *this, PutByKind::ByIdSloppy);
        break;
    case AccessType::PutByIdDirectStrict:
        resetPutBy(codeBlock, *this, PutByKind::ByIdDirectStrict);
        break;
    case AccessType::PutByIdDirectSloppy:
        resetPutBy(codeBlock, *this, PutByKind::ByIdDirectSloppy);
        break;
    case AccessType::PutByValStrict:
        resetPutBy(codeBlock, *this, PutByKind::ByValStrict);
        break;
    case AccessType::PutByValSloppy:
        resetPutBy(codeBlock, *this, PutByKind::ByValSloppy);
        break;
    case AccessType::PutByValDirectStrict:
        resetPutBy(codeBlock, *this, PutByKind::ByValDirectStrict);
        break;
    case AccessType::PutByValDirectSloppy:
        resetPutBy(codeBlock, *this, PutByKind::ByValDirectSloppy);
        break;
    case AccessType::DefinePrivateNameById:
        resetPutBy(codeBlock, *this, PutByKind::DefinePrivateNameById);
        break;
    case AccessType::SetPrivateNameById:
        resetPutBy(codeBlock, *this, PutByKind::SetPrivateNameById);
        break;
    case AccessType::DefinePrivateNameByVal:
        resetPutBy(codeBlock, *this, PutByKind::DefinePrivateNameByVal);
        break;
    case AccessType::SetPrivateNameByVal:
        resetPutBy(codeBlock, *this, PutByKind::SetPrivateNameByVal);
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
    case AccessType::DeleteByIdStrict:
        resetDelBy(codeBlock, *this, DelByKind::ByIdStrict);
        break;
    case AccessType::DeleteByIdSloppy:
        resetDelBy(codeBlock, *this, DelByKind::ByIdSloppy);
        break;
    case AccessType::DeleteByValStrict:
        resetDelBy(codeBlock, *this, DelByKind::ByValStrict);
        break;
    case AccessType::DeleteByValSloppy:
        resetDelBy(codeBlock, *this, DelByKind::ByValSloppy);
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
    if (!m_identifier) {
        Locker locker { m_bufferedStructuresLock };
        WTF::switchOn(m_bufferedStructures,
            [&](std::monostate) { },
            [&](Vector<StructureID>&) { },
            [&](Vector<std::tuple<StructureID, CacheableIdentifier>>& structures) {
                for (auto& [bufferedStructureID, bufferedCacheableIdentifier] : structures)
                    bufferedCacheableIdentifier.visitAggregate(visitor);
            });
    } else
        m_identifier.visitAggregate(visitor);

    if (m_stub)
        m_stub->visitAggregate(visitor);
}

DEFINE_VISIT_AGGREGATE(StructureStubInfo);

void StructureStubInfo::visitWeakReferences(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    VM& vm = codeBlock->vm();
    {
        Locker locker { m_bufferedStructuresLock };
        WTF::switchOn(m_bufferedStructures,
            [&](std::monostate) { },
            [&](Vector<StructureID>& structures) {
                structures.removeAllMatching([&](StructureID structureID) {
                    return !vm.heap.isMarked(structureID.decode());
                });
            },
            [&](Vector<std::tuple<StructureID, CacheableIdentifier>>& structures) {
                structures.removeAllMatching([&](auto& tuple) {
                    return !vm.heap.isMarked(std::get<0>(tuple).decode());
                });
            });
    }

    bool isValid = true;
    if (Structure* structure = inlineAccessBaseStructure())
        isValid &= vm.heap.isMarked(structure);
    if (m_stub)
        isValid &= m_stub->visitWeak(vm);
    if (m_inlinedHandler)
        isValid &= m_inlinedHandler->visitWeak(vm);
    if (m_handler) {
        RefPtr cursor = m_handler.get();
        while (cursor) {
            isValid &= cursor->visitWeak(vm);
            cursor = cursor->next();
        }
    }

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

    if (m_stub)
        m_stub->propagateTransitions(visitor);
}

template void StructureStubInfo::propagateTransitions(AbstractSlotVisitor&);
template void StructureStubInfo::propagateTransitions(SlotVisitor&);

CallLinkInfo* StructureStubInfo::callLinkInfoAt(const ConcurrentJSLocker& locker, unsigned index, const AccessCase& accessCase)
{
    if (!useDataIC) {
        if (!m_handler)
            return nullptr;
        return m_handler->callLinkInfoAt(locker, index);
    }

    if (m_inlinedHandler) {
        if (m_inlinedHandler->accessCase() == &accessCase)
            return m_inlinedHandler->callLinkInfoAt(locker, 0);
    }

    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            if (cursor->accessCase() == &accessCase)
                return cursor->callLinkInfoAt(locker, 0);
            cursor = cursor->next();
        }
    }
    return nullptr;
}

StubInfoSummary StructureStubInfo::summary(VM& vm) const
{
    StubInfoSummary takesSlowPath = StubInfoSummary::TakesSlowPath;
    StubInfoSummary simple = StubInfoSummary::Simple;
    if (auto* list = m_stub.get()) {
        for (unsigned i = 0; i < list->size(); ++i) {
            const AccessCase& access = list->at(i);
            if (access.doesCalls(vm)) {
                takesSlowPath = StubInfoSummary::TakesSlowPathAndMakesCalls;
                simple = StubInfoSummary::MakesCalls;
                break;
            }
        }
        if (list->size() == 1) {
            switch (list->at(0).type()) {
            case AccessCase::LoadMegamorphic:
            case AccessCase::IndexedMegamorphicLoad:
            case AccessCase::StoreMegamorphic:
            case AccessCase::IndexedMegamorphicStore:
            case AccessCase::InMegamorphic:
            case AccessCase::IndexedMegamorphicIn:
                return StubInfoSummary::Megamorphic;
            default:
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
    // m_inlinedHandler is not having special out-of-inline code, so we do not care.
    if (!m_handler)
        return false;

    auto* cursor = m_handler.get();
    while (cursor) {
        if (cursor->containsPC(pc))
            return true;
        cursor = cursor->next();
    }
    return false;
}

ALWAYS_INLINE void StructureStubInfo::setCacheType(const ConcurrentJSLockerBase&, CacheType newCacheType)
{
    m_cacheType = newCacheType;
}

static CodePtr<OperationPtrTag> slowOperationFromUnlinkedStructureStubInfo(const UnlinkedStructureStubInfo& unlinkedStubInfo)
{
    switch (unlinkedStubInfo.accessType) {
    case AccessType::DeleteByValStrict:
        return operationDeleteByValStrictOptimize;
    case AccessType::DeleteByValSloppy:
        return operationDeleteByValSloppyOptimize;
    case AccessType::DeleteByIdStrict:
        return operationDeleteByIdStrictOptimize;
    case AccessType::DeleteByIdSloppy:
        return operationDeleteByIdSloppyOptimize;
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
    case AccessType::PutByIdStrict:
        return operationPutByIdStrictOptimize;
    case AccessType::PutByIdSloppy:
        return operationPutByIdSloppyOptimize;
    case AccessType::PutByIdDirectStrict:
        return operationPutByIdDirectStrictOptimize;
    case AccessType::PutByIdDirectSloppy:
        return operationPutByIdDirectSloppyOptimize;
    case AccessType::PutByValStrict:
        return operationPutByValStrictOptimize;
    case AccessType::PutByValSloppy:
        return operationPutByValSloppyOptimize;
    case AccessType::PutByValDirectStrict:
        return operationDirectPutByValStrictOptimize;
    case AccessType::PutByValDirectSloppy:
        return operationDirectPutByValSloppyOptimize;
    case AccessType::DefinePrivateNameById:
        return operationPutByIdDefinePrivateFieldStrictOptimize;
    case AccessType::SetPrivateNameById:
        return operationPutByIdSetPrivateFieldStrictOptimize;
    case AccessType::DefinePrivateNameByVal:
        return operationPutByValDefinePrivateFieldOptimize;
    case AccessType::SetPrivateNameByVal:
        return operationPutByValSetPrivateFieldOptimize;
    case AccessType::SetPrivateBrand:
        return operationSetPrivateBrandOptimize;
    case AccessType::CheckPrivateBrand:
        return operationCheckPrivateBrandOptimize;
    }
    return { };
}

void StructureStubInfo::initializePredefinedRegisters()
{
    switch (accessType) {
    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy:
        m_baseGPR = BaselineJITRegisters::DelByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::DelByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::DelByVal::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::DelByVal::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::DelByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::DelByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::DelByVal::resultJSR.tagGPR();
#endif
        break;
    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy:
        m_baseGPR = BaselineJITRegisters::DelById::baseJSR.payloadGPR();
        m_extraGPR = InvalidGPRReg;
        m_valueGPR = BaselineJITRegisters::DelById::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::DelById::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::DelById::baseJSR.tagGPR();
        m_extraTagGPR = InvalidGPRReg;
        m_valueTagGPR = BaselineJITRegisters::DelById::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        m_baseGPR = BaselineJITRegisters::GetByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByVal::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByVal::stubInfoGPR;
        if (accessType == AccessType::GetByVal)
            m_arrayProfileGPR = BaselineJITRegisters::GetByVal::profileGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::GetByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::GetByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetByVal::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InstanceOf:
        prototypeIsKnownObject = false;
        m_baseGPR = BaselineJITRegisters::Instanceof::valueJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::Instanceof::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::Instanceof::protoJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::Instanceof::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::Instanceof::valueJSR.tagGPR();
        m_valueTagGPR = InvalidGPRReg;
        m_extraTagGPR = BaselineJITRegisters::Instanceof::protoJSR.tagGPR();
#endif
        break;
    case AccessType::InByVal:
    case AccessType::HasPrivateName:
    case AccessType::HasPrivateBrand:
        m_baseGPR = BaselineJITRegisters::InByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::InByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::InByVal::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::InByVal::stubInfoGPR;
        if (accessType == AccessType::InByVal)
            m_arrayProfileGPR = BaselineJITRegisters::InByVal::profileGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::InByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::InByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::InByVal::resultJSR.tagGPR();
#endif
        break;
    case AccessType::InById:
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
        m_extraGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::GetById::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetById::resultJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetById::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_extraTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::GetById::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetById::resultJSR.tagGPR();
#endif
        break;
    case AccessType::GetByIdWithThis:
        m_baseGPR = BaselineJITRegisters::GetByIdWithThis::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByIdWithThis::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByIdWithThis::thisJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByIdWithThis::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::GetByIdWithThis::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::GetByIdWithThis::resultJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::GetByIdWithThis::thisJSR.tagGPR();
#endif
        break;
    case AccessType::GetByValWithThis:
#if USE(JSVALUE64)
        m_baseGPR = BaselineJITRegisters::GetByValWithThis::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::GetByValWithThis::resultJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::GetByValWithThis::thisJSR.payloadGPR();
        m_extra2GPR = BaselineJITRegisters::GetByValWithThis::propertyJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::GetByValWithThis::stubInfoGPR;
        m_arrayProfileGPR = BaselineJITRegisters::GetByValWithThis::profileGPR;
#else
        // Registers are exhausted, we cannot have this IC on 32bit.
        RELEASE_ASSERT_NOT_REACHED();
#endif
        break;
    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById:
        m_extraGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::PutById::baseJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::PutById::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PutById::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_extraTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::PutById::baseJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::PutById::valueJSR.tagGPR();
#endif
        break;
    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal:
        m_baseGPR = BaselineJITRegisters::PutByVal::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::PutByVal::propertyJSR.payloadGPR();
        m_valueGPR = BaselineJITRegisters::PutByVal::valueJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PutByVal::stubInfoGPR;
        if (accessType != AccessType::DefinePrivateNameByVal && accessType != AccessType::SetPrivateNameByVal)
            m_arrayProfileGPR = BaselineJITRegisters::PutByVal::profileGPR;
#if USE(JSVALUE32_64)
        m_baseTagGPR = BaselineJITRegisters::PutByVal::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::PutByVal::propertyJSR.tagGPR();
        m_valueTagGPR = BaselineJITRegisters::PutByVal::valueJSR.tagGPR();
#endif
        break;
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        m_valueGPR = InvalidGPRReg;
        m_baseGPR = BaselineJITRegisters::PrivateBrand::baseJSR.payloadGPR();
        m_extraGPR = BaselineJITRegisters::PrivateBrand::propertyJSR.payloadGPR();
        m_stubInfoGPR = BaselineJITRegisters::PrivateBrand::stubInfoGPR;
#if USE(JSVALUE32_64)
        m_valueTagGPR = InvalidGPRReg;
        m_baseTagGPR = BaselineJITRegisters::PrivateBrand::baseJSR.tagGPR();
        m_extraTagGPR = BaselineJITRegisters::PrivateBrand::propertyJSR.tagGPR();
#endif
        break;
    }
}

void StructureStubInfo::initializeFromUnlinkedStructureStubInfo(VM& vm, CodeBlock* codeBlock, const BaselineUnlinkedStructureStubInfo& unlinkedStubInfo)
{
    ASSERT(!isCompilationThread());
    accessType = unlinkedStubInfo.accessType;
    preconfiguredCacheType = unlinkedStubInfo.preconfiguredCacheType;
    switch (preconfiguredCacheType) {
    case CacheType::ArrayLength:
        m_cacheType = CacheType::ArrayLength;
        break;
    default:
        break;
    }
    doneLocation = unlinkedStubInfo.doneLocation;
    m_identifier = unlinkedStubInfo.m_identifier;
    m_globalObject = codeBlock->globalObject();
    callSiteIndex = CallSiteIndex(BytecodeIndex(unlinkedStubInfo.bytecodeIndex.offset()));
    codeOrigin = CodeOrigin(unlinkedStubInfo.bytecodeIndex);
    if (Options::useHandlerIC())
        initializeWithUnitHandler(codeBlock, InlineCacheCompiler::generateSlowPathHandler(vm, accessType));
    else {
        initializeWithUnitHandler(codeBlock, InlineCacheHandler::createNonHandlerSlowPath(unlinkedStubInfo.slowPathStartLocation));
        slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    }
    propertyIsInt32 = unlinkedStubInfo.propertyIsInt32;
    canBeMegamorphic = unlinkedStubInfo.canBeMegamorphic;
    useDataIC = true;

    if (unlinkedStubInfo.canBeMegamorphic)
        bufferingCountdown = 1;

    usedRegisters = RegisterSetBuilder::stubUnavailableRegisters().buildScalarRegisterSet();

    m_slowOperation = slowOperationFromUnlinkedStructureStubInfo(unlinkedStubInfo);
    initializePredefinedRegisters();
}

#if ENABLE(DFG_JIT)
void StructureStubInfo::initializeFromDFGUnlinkedStructureStubInfo(CodeBlock* codeBlock, const DFG::UnlinkedStructureStubInfo& unlinkedStubInfo)
{
    ASSERT(!isCompilationThread());
    accessType = unlinkedStubInfo.accessType;
    preconfiguredCacheType = unlinkedStubInfo.preconfiguredCacheType;
    switch (preconfiguredCacheType) {
    case CacheType::ArrayLength:
        m_cacheType = CacheType::ArrayLength;
        break;
    default:
        break;
    }
    doneLocation = unlinkedStubInfo.doneLocation;
    m_identifier = unlinkedStubInfo.m_identifier;
    callSiteIndex = unlinkedStubInfo.callSiteIndex;
    codeOrigin = unlinkedStubInfo.codeOrigin;
    if (codeOrigin.inlineCallFrame())
        m_globalObject = baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame())->globalObject();
    else
        m_globalObject = codeBlock->globalObject();
    if (Options::useHandlerIC())
        initializeWithUnitHandler(codeBlock, InlineCacheCompiler::generateSlowPathHandler(codeBlock->vm(), accessType));
    else {
        initializeWithUnitHandler(codeBlock, InlineCacheHandler::createNonHandlerSlowPath(unlinkedStubInfo.slowPathStartLocation));
        slowPathStartLocation = unlinkedStubInfo.slowPathStartLocation;
    }

    propertyIsInt32 = unlinkedStubInfo.propertyIsInt32;
    propertyIsSymbol = unlinkedStubInfo.propertyIsSymbol;
    propertyIsString = unlinkedStubInfo.propertyIsString;
    prototypeIsKnownObject = unlinkedStubInfo.prototypeIsKnownObject;
    canBeMegamorphic = unlinkedStubInfo.canBeMegamorphic;
    useDataIC = true;

    if (unlinkedStubInfo.canBeMegamorphic)
        bufferingCountdown = 1;

    usedRegisters = RegisterSetBuilder::stubUnavailableRegisters().buildScalarRegisterSet();

    m_slowOperation = slowOperationFromUnlinkedStructureStubInfo(unlinkedStubInfo);
    initializePredefinedRegisters();
}
#endif

void StructureStubInfo::setInlinedHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    ASSERT(!m_inlinedHandler);
    VM& vm = codeBlock->vm();
    m_inlinedHandler = WTFMove(handler);
    m_inlinedHandler->addOwner(codeBlock);
    switch (m_inlinedHandler->cacheType()) {
    case CacheType::GetByIdSelf: {
        m_inlineAccessBaseStructureID.set(vm, codeBlock, m_inlinedHandler->structureID().decode());
        byIdSelfOffset = m_inlinedHandler->offset();
        break;
    }
    case CacheType::GetByIdPrototype: {
        m_inlineAccessBaseStructureID.set(vm, codeBlock, m_inlinedHandler->structureID().decode());
        byIdSelfOffset = m_inlinedHandler->offset();
        m_inlineHolder = m_inlinedHandler->holder();
        break;
    }
    case CacheType::PutByIdReplace: {
        m_inlineAccessBaseStructureID.set(vm, codeBlock, m_inlinedHandler->structureID().decode());
        byIdSelfOffset = m_inlinedHandler->offset();
        break;
    }
    case CacheType::InByIdSelf: {
        m_inlineAccessBaseStructureID.set(vm, codeBlock, m_inlinedHandler->structureID().decode());
        byIdSelfOffset = m_inlinedHandler->offset();
        break;
    }
    case CacheType::ArrayLength:
    case CacheType::StringLength:
    case CacheType::Unset:
    case CacheType::Stub:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void StructureStubInfo::clearInlinedHandler(CodeBlock* codeBlock)
{
    ASSERT(useHandlerIC());
    m_inlinedHandler->removeOwner(codeBlock);
    m_inlinedHandler = nullptr;
    m_inlineAccessBaseStructureID.clear();
}

void StructureStubInfo::initializeWithUnitHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    if (useHandlerIC()) {
        if (m_inlinedHandler)
            clearInlinedHandler(codeBlock);
        ASSERT(!m_inlinedHandler);
        if (m_handler)
            m_handler->removeOwner(codeBlock);
        m_handler = WTFMove(handler);
        m_handler->addOwner(codeBlock);
    } else {
        ASSERT(!m_inlinedHandler);
        m_handler = WTFMove(handler);
    }
}

void StructureStubInfo::prependHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler, bool isMegamorphic)
{
    ASSERT(useHandlerIC());
    if (isMegamorphic) {
        initializeWithUnitHandler(codeBlock, WTFMove(handler));
        return;
    }

    if (!m_inlinedHandler) {
        if (preconfiguredCacheType != CacheType::Unset && preconfiguredCacheType == handler->cacheType()) {
            setInlinedHandler(codeBlock, WTFMove(handler));
            return;
        }
    }

    handler->setNext(WTFMove(m_handler));
    m_handler = WTFMove(handler);
    m_handler->addOwner(codeBlock);
}

void StructureStubInfo::rewireStubAsJumpInAccess(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    CodeLocationLabel label { handler->callTarget() };
    initializeWithUnitHandler(codeBlock, WTFMove(handler));
    if (!useDataIC)
        CCallHelpers::replaceWithJump(startLocation.retagged<JSInternalPtrTag>(), label);
}

void StructureStubInfo::resetStubAsJumpInAccess(CodeBlock* codeBlock)
{
    if (useHandlerIC()) {
        if (m_inlinedHandler)
            clearInlinedHandler(codeBlock);
        auto* cursor = m_handler.get();
        while (cursor) {
            cursor->removeOwner(codeBlock);
            cursor = cursor->next();
        }
        m_handler = InlineCacheCompiler::generateSlowPathHandler(codeBlock->vm(), accessType);
        return;
    }

    if (useDataIC)
        m_inlineAccessBaseStructureID.clear(); // Clear out the inline access code.
    rewireStubAsJumpInAccess(codeBlock, InlineCacheHandler::createNonHandlerSlowPath(slowPathStartLocation));
}

#if ASSERT_ENABLED
void StructureStubInfo::checkConsistency()
{
    switch (accessType) {
    case AccessType::GetByIdWithThis:
        // We currently use a union for both "thisGPR" and "propertyGPR". If this were
        // not the case, we'd need to take one of them out of the union.
        RELEASE_ASSERT(hasConstantIdentifier(accessType));
        break;
    case AccessType::GetByValWithThis:
    default:
        break;
    }
}
#endif // ASSERT_ENABLED

#endif // ENABLE(JIT)

} // namespace JSC
