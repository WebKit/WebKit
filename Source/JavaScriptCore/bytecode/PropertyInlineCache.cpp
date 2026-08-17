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
#include "PropertyInlineCache.h"

#include "BaselineJITRegisters.h"
#include "CacheableIdentifierInlines.h"
#include "DFGJITCode.h"
#include "InlineCacheCompiler.h"
#include "Repatch.h"

namespace JSC {

#if ENABLE(JIT)

namespace PropertyInlineCacheInternal {
static constexpr bool verbose = false;
}

PropertyInlineCache::~PropertyInlineCache() = default;

RepatchingPropertyInlineCache::RepatchingPropertyInlineCache()
    : RepatchingPropertyInlineCache(AccessType::GetById, { })
{
}

RepatchingPropertyInlineCache::RepatchingPropertyInlineCache(AccessType accessType, CodeOrigin codeOrigin)
    : PropertyInlineCache(PropertyInlineCacheType::Repatching, accessType, codeOrigin)
    , bufferingCountdown(Options::initialRepatchBufferingCountdown())
{
}

RepatchingPropertyInlineCache::~RepatchingPropertyInlineCache() = default;

void PropertyInlineCache::initGetByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::GetByIdSelf);
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void PropertyInlineCache::initArrayLength(const ConcurrentJSLockerBase& locker)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::ArrayLength);
}

void PropertyInlineCache::initStringLength(const ConcurrentJSLockerBase& locker)
{
    ASSERT(m_cacheType == CacheType::Unset);
    setCacheType(locker, CacheType::StringLength);
}

void PropertyInlineCache::initPutByIdReplace(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::PutByIdReplace);
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void PropertyInlineCache::initInByIdSelf(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock, Structure* inlineAccessBaseStructure, PropertyOffset offset)
{
    ASSERT(m_cacheType == CacheType::Unset);
    ASSERT(hasConstantIdentifier(accessType));
    setCacheType(locker, CacheType::InByIdSelf);
    m_inlineAccessBaseStructureID.set(codeBlock->vm(), codeBlock, inlineAccessBaseStructure);
    byIdSelfOffset = offset;
}

void PropertyInlineCache::deref()
{
    if (auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(*this))
        repatchingIC->m_stub.reset();
}

void PropertyInlineCache::aboutToDie()
{
    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->m_inlinedHandler->aboutToDie();
    }

    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            cursor->aboutToDie();
            cursor = cursor->next();
        }
    }
}

AccessGenerationResult PropertyInlineCache::upgradeForPolyProtoIfNecessary(const GCSafeConcurrentJSLocker&, VM&, CodeBlock*, const Vector<AccessCase*, 16>& list, AccessCase& caseToAdd)
{
    // This method will add the casesToAdd to the list one at a time while preserving the
    // invariants:
    // - If a newly added case canReplace() any existing case, then the existing case is removed before
    //   the new case is added. Removal doesn't change order of the list. Any number of existing cases
    //   can be removed via the canReplace() rule.
    // - Cases in the list always appear in ascending order of time of addition. Therefore, if you
    //   cascade through the cases in reverse order, you will get the most recent cases first.
    // - If this method fails (returns null, doesn't add the cases), then both the previous case list
    //   and the previous stub are kept intact and the new cases are destroyed. It's OK to attempt to
    //   add more things after failure.

    if (accessType != AccessType::InstanceOf) {
        bool shouldReset = false;
        AccessGenerationResult resetResult(AccessGenerationResult::ResetStubAndFireWatchpoints);
        auto considerPolyProtoReset = [&] (Structure* a, Structure* b) {
            if (Structure::shouldConvertToPolyProto(a, b)) {
                // For now, we only reset if this is our first time invalidating this watchpoint.
                // The reason we don't immediately fire this watchpoint is that we may be already
                // watching the poly proto watchpoint, which if fired, would destroy us. We let
                // the person handling the result to do a delayed fire.
                ASSERT(a->rareData()->sharedPolyProtoWatchpoint().get() == b->rareData()->sharedPolyProtoWatchpoint().get());
                if (a->rareData()->sharedPolyProtoWatchpoint()->isStillValid()) {
                    shouldReset = true;
                    resetResult.addWatchpointToFire(*a->rareData()->sharedPolyProtoWatchpoint(), StringFireDetail("Detected poly proto optimization opportunity."));
                }
            }
        };

        for (auto& existingCase : list) {
            Structure* a = caseToAdd.structure();
            Structure* b = existingCase->structure();
            considerPolyProtoReset(a, b);
        }

        if (shouldReset)
            return resetResult;
    }
    return AccessGenerationResult::Buffered;
}

AccessGenerationResult PropertyInlineCache::addAccessCase(const GCSafeConcurrentJSLocker& locker, JSGlobalObject* globalObject, CodeBlock* codeBlock, ECMAMode ecmaMode, CacheableIdentifier ident, RefPtr<AccessCase> accessCase)
{
    checkConsistency();

    VM& vm = codeBlock->vm();
    ASSERT(vm.heap.isDeferred());

    if (!accessCase)
        return AccessGenerationResult::GaveUp;

    AccessGenerationResult result = ([&](Ref<AccessCase>&& accessCase) -> AccessGenerationResult {
        dataLogLnIf(PropertyInlineCacheInternal::verbose, "Adding access case: ", accessCase);

        if (is<HandlerPropertyInlineCache>(*this)) {
            auto list = listedAccessCases(locker);
            auto result = upgradeForPolyProtoIfNecessary(locker, vm, codeBlock, list, accessCase.get());
            dataLogLnIf(PropertyInlineCacheInternal::verbose, "Had stub, result: ", result);

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered())
                return result;
            setCacheType(locker, CacheType::Stub);

            RELEASE_ASSERT(!result.generatedSomeCode());

            InlineCacheCompiler compiler(codeBlock->jitType(), vm, globalObject, ecmaMode, *this);
            return compiler.compileHandler(locker, WTF::move(list), codeBlock, accessCase.get());
        }

        auto& repatchingIC = downcast<RepatchingPropertyInlineCache>(*this);
        AccessGenerationResult result;
        if (repatchingIC.m_stub) {
            result = repatchingIC.m_stub->addCases(locker, vm, codeBlock, *this, nullptr, accessCase);
            dataLogLnIf(PropertyInlineCacheInternal::verbose, "Had stub, result: ", result);

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                repatchingIC.clearBufferedStructures();
                return result;
            }
        } else {
            std::unique_ptr<PolymorphicAccess> access = makeUnique<PolymorphicAccess>();
            result = access->addCases(locker, vm, codeBlock, *this, AccessCase::fromPropertyInlineCache(vm, codeBlock, ident, *this), accessCase);

            dataLogLnIf(PropertyInlineCacheInternal::verbose, "Created stub, result: ", result);

            if (result.shouldResetStubAndFireWatchpoints())
                return result;

            if (!result.buffered()) {
                repatchingIC.clearBufferedStructures();
                return result;
            }

            setCacheType(locker, CacheType::Stub);
            repatchingIC.m_stub = WTF::move(access);
        }

        ASSERT(m_cacheType == CacheType::Stub);
        RELEASE_ASSERT(!result.generatedSomeCode());

        // The buffering countdown tells us if we should be repatching now.
        if (repatchingIC.bufferingCountdown) {
            dataLogLnIf(PropertyInlineCacheInternal::verbose, "Countdown is too high: ", repatchingIC.bufferingCountdown, ".");
            return result;
        }

        // Forget the buffered structures so that all future attempts to cache get fully handled by the
        // PolymorphicAccess.
        repatchingIC.clearBufferedStructures();

        InlineCacheCompiler compiler(codeBlock->jitType(), vm, globalObject, ecmaMode, *this);
        result = compiler.compile(locker, *repatchingIC.m_stub, codeBlock);

        dataLogLnIf(PropertyInlineCacheInternal::verbose, "Regeneration result: ", result);

        RELEASE_ASSERT(!result.buffered());

        if (!result.generatedSomeCode())
            return result;

        // Repatching IC: When we first transition to becoming a Stub, we might still be running the inline
        // access code. That's because when we first transition to becoming a Stub, we may
        // be buffered, and we have not yet generated any code. Once the Stub finally generates
        // code, we're no longer running the inline access code, so we can then clear out
        // m_inlineAccessBaseStructureID. The reason we don't clear m_inlineAccessBaseStructureID while
        // we're buffered is because we rely on it to reset during GC if m_inlineAccessBaseStructureID
        // is collected.
        m_inlineAccessBaseStructureID.clear();

        // If we generated some code then we don't want to attempt to repatch in the future until we
        // gather enough cases.
        repatchingIC.bufferingCountdown = Options::repatchBufferingCountdown();
        return result;
    })(accessCase.releaseNonNull());
    if (result.generatedSomeCode()) {
        if (is<HandlerPropertyInlineCache>(*this))
            prependHandler(codeBlock, Ref { *result.handler() }, result.generatedMegamorphicCode());
        else
            rewireStubAsJumpInAccess(codeBlock, Ref { *result.handler() });
    }

    vm.writeBarrier(codeBlock);
    return result;
}

void PropertyInlineCache::reset(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    m_inlineAccessBaseStructureID.clear();
    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->clearInlinedHandler(codeBlock);
    } else
        downcast<RepatchingPropertyInlineCache>(*this).clearBufferedStructures();

    if (m_cacheType == CacheType::Unset)
        return;

    // This can be called from GC destructor calls, so we don't try to do a full dump
    // of the CodeBlock.
    dataLogLnIf(Options::verboseOSR(), "Clearing structure cache (kind ", static_cast<int>(accessType), ") in ", RawPointer(codeBlock), ".");

    switch (accessType) {
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
void RepatchingPropertyInlineCache::visitBufferedStructures(Visitor& visitor)
{
    Locker locker { m_bufferedStructuresLock };
    WTF::switchOn(m_bufferedStructures,
        [&](std::monostate) { },
        [&](Vector<StructureID>&) { },
        [&](Vector<std::tuple<StructureID, CacheableIdentifier>>& structures) {
            for (auto& [bufferedStructureID, bufferedCacheableIdentifier] : structures)
                bufferedCacheableIdentifier.visitAggregate(visitor);
        });
}

void RepatchingPropertyInlineCache::pruneDeadBufferedStructures(VM& vm)
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

template<typename Visitor>
void PropertyInlineCache::visitAggregateImpl(Visitor& visitor)
{
    m_identifier.visitAggregate(visitor);

    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->m_inlinedHandler->visitAggregate(visitor);
    }
    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            cursor->visitAggregate(visitor);
            cursor = cursor->next();
        }
    }

    if (auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(*this)) {
        repatchingIC->visitBufferedStructures(visitor);
        if (repatchingIC->m_stub)
            repatchingIC->m_stub->visitAggregate(visitor);
    }
}

DEFINE_VISIT_AGGREGATE(PropertyInlineCache);

void PropertyInlineCache::reconcileWeakReferencesAtGCEnd(const ConcurrentJSLockerBase& locker, CodeBlock* codeBlock)
{
    VM& vm = codeBlock->vm();
    bool isValid = true;
    if (Structure* structure = inlineAccessBaseStructure())
        isValid &= vm.heap.isMarked(structure);

    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            isValid &= handlerIC->m_inlinedHandler->reconcileWeakReferencesAtGCEnd(vm);
    }
    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            isValid &= cursor->reconcileWeakReferencesAtGCEnd(vm);
            cursor = cursor->next();
        }
    }

    if (auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(*this)) {
        repatchingIC->pruneDeadBufferedStructures(vm);
        if (repatchingIC->m_stub)
            isValid &= repatchingIC->m_stub->isStillLive(vm);
    }

    if (isValid)
        return;

    reset(locker, codeBlock);
    resetByGC = true;
}

template<typename Visitor>
void PropertyInlineCache::propagateTransitions(Visitor& visitor)
{
    if (Structure* structure = inlineAccessBaseStructure())
        structure->markIfCheap(visitor);

    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->m_inlinedHandler->propagateTransitions(visitor);
        if (auto* cursor = m_handler.get()) {
            while (cursor) {
                cursor->propagateTransitions(visitor);
                cursor = cursor->next();
            }
        }
    } else if (auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(*this)) {
        if (repatchingIC->m_stub)
            repatchingIC->m_stub->propagateTransitions(visitor);
    }
}

template void PropertyInlineCache::propagateTransitions(AbstractSlotVisitor&);
template void PropertyInlineCache::propagateTransitions(SlotVisitor&);

CallLinkInfo* PropertyInlineCache::callLinkInfoAt(const ConcurrentJSLocker& locker, unsigned index, const AccessCase& accessCase)
{
    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler) {
            if (handlerIC->m_inlinedHandler->accessCase() == &accessCase)
                return downcast<InlineCacheHandlerWithJSCall>(*handlerIC->m_inlinedHandler).callLinkInfo(locker);
        }

        if (auto* cursor = m_handler.get()) {
            while (cursor) {
                if (cursor->accessCase() == &accessCase)
                    return downcast<InlineCacheHandlerWithJSCall>(*cursor).callLinkInfo(locker);
                cursor = cursor->next();
            }
        }
        return nullptr;
    }

    if (!m_handler)
        return nullptr;
    if (!m_handler->stubRoutine())
        return nullptr;
    return m_handler->stubRoutine()->callLinkInfoAt(locker, index);
}

PropertyInlineCacheSummary PropertyInlineCache::summary(const ConcurrentJSLocker& locker, VM& vm) const
{
    PropertyInlineCacheSummary takesSlowPath = PropertyInlineCacheSummary::TakesSlowPath;
    PropertyInlineCacheSummary simple = PropertyInlineCacheSummary::Simple;
    auto list = listedAccessCases(locker);
    for (unsigned i = 0; i < list.size(); ++i) {
        AccessCase& access = *list.at(i);
        if (access.doesCalls(vm)) {
            takesSlowPath = PropertyInlineCacheSummary::TakesSlowPathAndMakesCalls;
            simple = PropertyInlineCacheSummary::MakesCalls;
            break;
        }
    }
    if (list.size() == 1) {
        switch (list.at(0)->type()) {
        case AccessCase::LoadMegamorphic:
        case AccessCase::IndexedMegamorphicLoad:
        case AccessCase::StoreMegamorphic:
        case AccessCase::IndexedMegamorphicStore:
        case AccessCase::InMegamorphic:
        case AccessCase::IndexedMegamorphicIn:
            return PropertyInlineCacheSummary::Megamorphic;
        default:
            break;
        }
    }

    if (tookSlowPath || sawNonCell)
        return takesSlowPath;

    if (!everConsidered)
        return PropertyInlineCacheSummary::NoInformation;

    return simple;
}

PropertyInlineCacheSummary PropertyInlineCache::summary(const ConcurrentJSLocker& locker, VM& vm, const PropertyInlineCache* propertyCache)
{
    if (!propertyCache)
        return PropertyInlineCacheSummary::NoInformation;

    return propertyCache->summary(locker, vm);
}

bool PropertyInlineCache::containsPC(void* pc) const
{
    // m_inlinedHandler is not having special out-of-inline code, so we do not care.
    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            if (cursor->containsPC(pc))
                return true;
            cursor = cursor->next();
        }
    }
    return false;
}

ALWAYS_INLINE void PropertyInlineCache::setCacheType(const ConcurrentJSLockerBase&, CacheType newCacheType)
{
    m_cacheType = newCacheType;
}

static CodePtr<OperationPtrTag> NODELETE slowOperationFromUnlinkedPropertyInlineCache(const UnlinkedPropertyInlineCache& unlinkedPropertyCache)
{
    switch (unlinkedPropertyCache.accessType) {
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

PropertyInlineCache::Registers PropertyInlineCache::registers() const
{
    if (auto* repatching = dynamicDowncast<RepatchingPropertyInlineCache>(*this))
        return repatching->m_registers;

    Registers registers;
    switch (accessType) {
    case AccessType::DeleteByValStrict:
    case AccessType::DeleteByValSloppy:
        registers.baseGPR = BaselineJITRegisters::DelByVal::baseJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::DelByVal::propertyJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::DelByVal::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::DelByVal::propertyCacheGPR;
        break;
    case AccessType::DeleteByIdStrict:
    case AccessType::DeleteByIdSloppy:
        registers.baseGPR = BaselineJITRegisters::DelById::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::DelById::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::DelById::propertyCacheGPR;
        break;
    case AccessType::GetByVal:
    case AccessType::GetPrivateName:
        registers.baseGPR = BaselineJITRegisters::GetByVal::baseJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::GetByVal::propertyJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::GetByVal::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::GetByVal::propertyCacheGPR;
        if (accessType == AccessType::GetByVal)
            registers.arrayProfileGPR = BaselineJITRegisters::GetByVal::profileGPR;
        break;
    case AccessType::InstanceOf:
        registers.baseGPR = BaselineJITRegisters::Instanceof::valueJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::Instanceof::resultJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::Instanceof::protoJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::Instanceof::propertyCacheGPR;
        break;
    case AccessType::InByVal:
    case AccessType::HasPrivateName:
    case AccessType::HasPrivateBrand:
        registers.baseGPR = BaselineJITRegisters::InByVal::baseJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::InByVal::propertyJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::InByVal::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::InByVal::propertyCacheGPR;
        if (accessType == AccessType::InByVal)
            registers.arrayProfileGPR = BaselineJITRegisters::InByVal::profileGPR;
        break;
    case AccessType::InById:
        registers.baseGPR = BaselineJITRegisters::InById::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::InById::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::InById::propertyCacheGPR;
        break;
    case AccessType::GetByIdDirect:
    case AccessType::GetById:
    case AccessType::GetPrivateNameById:
        registers.baseGPR = BaselineJITRegisters::GetById::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::GetById::resultJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::GetById::propertyCacheGPR;
        break;
    case AccessType::GetByIdWithThis:
        registers.baseGPR = BaselineJITRegisters::GetByIdWithThis::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::GetByIdWithThis::resultJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::GetByIdWithThis::thisJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::GetByIdWithThis::propertyCacheGPR;
        break;
    case AccessType::GetByValWithThis:
        registers.baseGPR = BaselineJITRegisters::GetByValWithThis::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::GetByValWithThis::resultJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::GetByValWithThis::thisJSR.payloadGPR();
        registers.extra2GPR = BaselineJITRegisters::GetByValWithThis::propertyJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::GetByValWithThis::propertyCacheGPR;
        registers.arrayProfileGPR = BaselineJITRegisters::GetByValWithThis::profileGPR;
        break;
    case AccessType::PutByIdStrict:
    case AccessType::PutByIdSloppy:
    case AccessType::PutByIdDirectStrict:
    case AccessType::PutByIdDirectSloppy:
    case AccessType::DefinePrivateNameById:
    case AccessType::SetPrivateNameById:
        registers.baseGPR = BaselineJITRegisters::PutById::baseJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::PutById::valueJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::PutById::propertyCacheGPR;
        break;
    case AccessType::PutByValStrict:
    case AccessType::PutByValSloppy:
    case AccessType::PutByValDirectStrict:
    case AccessType::PutByValDirectSloppy:
    case AccessType::DefinePrivateNameByVal:
    case AccessType::SetPrivateNameByVal:
        registers.baseGPR = BaselineJITRegisters::PutByVal::baseJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::PutByVal::propertyJSR.payloadGPR();
        registers.valueGPR = BaselineJITRegisters::PutByVal::valueJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::PutByVal::propertyCacheGPR;
        if (accessType != AccessType::DefinePrivateNameByVal && accessType != AccessType::SetPrivateNameByVal)
            registers.arrayProfileGPR = BaselineJITRegisters::PutByVal::profileGPR;
        break;
    case AccessType::SetPrivateBrand:
    case AccessType::CheckPrivateBrand:
        registers.baseGPR = BaselineJITRegisters::PrivateBrand::baseJSR.payloadGPR();
        registers.extraGPR = BaselineJITRegisters::PrivateBrand::propertyJSR.payloadGPR();
        registers.propertyCacheGPR = BaselineJITRegisters::PrivateBrand::propertyCacheGPR;
        break;
    }
    return registers;
}

void HandlerPropertyInlineCache::initializeFromUnlinkedPropertyInlineCache(VM& vm, CodeBlock* codeBlock, const BaselineUnlinkedPropertyInlineCache& unlinkedPropertyCache)
{
    ASSERT(!isCompilationThread());
    accessType = unlinkedPropertyCache.accessType;
    preconfiguredCacheType = unlinkedPropertyCache.preconfiguredCacheType;
    switch (preconfiguredCacheType) {
    case CacheType::ArrayLength:
        m_cacheType = CacheType::ArrayLength;
        break;
    default:
        break;
    }
    doneLocation = unlinkedPropertyCache.doneLocation;
    m_identifier = unlinkedPropertyCache.m_identifier;
    m_globalObject = codeBlock->globalObject();
    callSiteIndex = CallSiteIndex(BytecodeIndex(unlinkedPropertyCache.bytecodeIndex.offset()));
    codeOrigin = CodeOrigin(unlinkedPropertyCache.bytecodeIndex);
    initializeWithUnitHandler(codeBlock, InlineCacheCompiler::generateSlowPathHandler(vm, accessType));
    propertyIsInt32 = unlinkedPropertyCache.propertyIsInt32;
    canBeMegamorphic = unlinkedPropertyCache.canBeMegamorphic;

    m_slowOperation = slowOperationFromUnlinkedPropertyInlineCache(unlinkedPropertyCache);
}

#if ENABLE(DFG_JIT)
void HandlerPropertyInlineCache::initializeFromDFGUnlinkedPropertyInlineCache(CodeBlock* codeBlock, const DFG::UnlinkedPropertyInlineCache& unlinkedPropertyCache)
{
    ASSERT(!isCompilationThread());
    accessType = unlinkedPropertyCache.accessType;
    preconfiguredCacheType = unlinkedPropertyCache.preconfiguredCacheType;
    switch (preconfiguredCacheType) {
    case CacheType::ArrayLength:
        m_cacheType = CacheType::ArrayLength;
        break;
    default:
        break;
    }
    doneLocation = unlinkedPropertyCache.doneLocation;
    m_identifier = unlinkedPropertyCache.m_identifier;
    callSiteIndex = unlinkedPropertyCache.callSiteIndex;
    codeOrigin = unlinkedPropertyCache.codeOrigin;
    if (codeOrigin.inlineCallFrame())
        m_globalObject = baselineCodeBlockForInlineCallFrame(codeOrigin.inlineCallFrame())->globalObject();
    else
        m_globalObject = codeBlock->globalObject();
    initializeWithUnitHandler(codeBlock, InlineCacheCompiler::generateSlowPathHandler(codeBlock->vm(), accessType));

    propertyIsInt32 = unlinkedPropertyCache.propertyIsInt32;
    propertyIsSymbol = unlinkedPropertyCache.propertyIsSymbol;
    propertyIsString = unlinkedPropertyCache.propertyIsString;
    prototypeIsKnownObject = unlinkedPropertyCache.prototypeIsKnownObject;
    canBeMegamorphic = unlinkedPropertyCache.canBeMegamorphic;

    m_slowOperation = slowOperationFromUnlinkedPropertyInlineCache(unlinkedPropertyCache);
}
#endif

void HandlerPropertyInlineCache::setInlinedHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    ASSERT(!m_inlinedHandler);
    VM& vm = codeBlock->vm();
    m_inlinedHandler = WTF::move(handler);
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

void HandlerPropertyInlineCache::clearInlinedHandler(CodeBlock* codeBlock)
{
    m_inlinedHandler->removeOwner(codeBlock);
    m_inlinedHandler = nullptr;
    m_inlineAccessBaseStructureID.clear();
}

void PropertyInlineCache::initializeWithUnitHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->clearInlinedHandler(codeBlock);
        ASSERT(!handlerIC->m_inlinedHandler);
        if (m_handler)
            m_handler->removeOwner(codeBlock);
        m_handler = WTF::move(handler);
        m_handler->addOwner(codeBlock);
    } else {
        m_handler = WTF::move(handler);
    }
}

void PropertyInlineCache::prependHandler(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler, bool isMegamorphic)
{
    auto& handlerIC = downcast<HandlerPropertyInlineCache>(*this);
    if (isMegamorphic) {
        initializeWithUnitHandler(codeBlock, WTF::move(handler));
        return;
    }

    if (!handlerIC.m_inlinedHandler) {
        if (preconfiguredCacheType != CacheType::Unset && preconfiguredCacheType == handler->cacheType()) {
            handlerIC.setInlinedHandler(codeBlock, WTF::move(handler));
            return;
        }
    }

    handler->setNext(WTF::move(m_handler));
    m_handler = WTF::move(handler);
    m_handler->addOwner(codeBlock);
}

void PropertyInlineCache::rewireStubAsJumpInAccess(CodeBlock* codeBlock, Ref<InlineCacheHandler>&& handler)
{
    ASSERT(!isHandlerIC());
    CodeLocationLabel label { handler->callTarget() };
    initializeWithUnitHandler(codeBlock, WTF::move(handler));
    CCallHelpers::replaceWithJump(downcast<RepatchingPropertyInlineCache>(*this).startLocation.retagged<JSInternalPtrTag>(), label);
}

void PropertyInlineCache::resetStubAsJumpInAccess(CodeBlock* codeBlock)
{
    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler)
            handlerIC->clearInlinedHandler(codeBlock);
        auto* cursor = m_handler.get();
        while (cursor) {
            cursor->removeOwner(codeBlock);
            cursor = cursor->next();
        }
        m_handler = InlineCacheCompiler::generateSlowPathHandler(codeBlock->vm(), accessType);
        return;
    }

    rewireStubAsJumpInAccess(codeBlock, InlineCacheHandler::createNonHandlerSlowPath(downcast<RepatchingPropertyInlineCache>(*this).slowPathStartLocation));
}

Vector<AccessCase*, 16> PropertyInlineCache::listedAccessCases(const AbstractLocker&) const
{
    Vector<AccessCase*, 16> cases;
    if (auto* repatchingIC = dynamicDowncast<RepatchingPropertyInlineCache>(*this)) {
        if (repatchingIC->m_stub) {
            for (unsigned i = 0; i < repatchingIC->m_stub->size(); ++i)
                cases.append(&repatchingIC->m_stub->at(i));
            return cases;
        }
    }

    if (auto* handlerIC = dynamicDowncast<HandlerPropertyInlineCache>(*this)) {
        if (handlerIC->m_inlinedHandler) {
            if (auto* access = handlerIC->m_inlinedHandler->accessCase())
                cases.append(access);
        }
    }

    if (auto* cursor = m_handler.get()) {
        while (cursor) {
            if (auto* access = cursor->accessCase())
                cases.append(access);
            cursor = cursor->next();
        }
    }

    return cases;
}

#if ASSERT_ENABLED
void PropertyInlineCache::checkConsistency()
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
