/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
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
#include "Repatch.h"

#if ENABLE(JIT)

#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CallFrameShuffler.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "FTLThunks.h"
#include "GCAwareJITStubRoutine.h"
#include "GetterSetter.h"
#include "JIT.h"
#include "JITInlines.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "PolymorphicAccess.h"
#include "ScratchRegisterAllocator.h"
#include "StackAlignment.h"
#include "StructureRareDataInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "ThunkGenerators.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

// Beware: in this code, it is not safe to assume anything about the following registers
// that would ordinarily have well-known values:
// - tagTypeNumberRegister
// - tagMaskRegister

static FunctionPtr readCallTarget(CodeBlock* codeBlock, CodeLocationCall call)
{
    FunctionPtr result = MacroAssembler::readCallTarget(call);
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITCode::FTLJIT) {
        return FunctionPtr(codeBlock->vm()->ftlThunks->keyForSlowPathCallThunk(
            MacroAssemblerCodePtr::createFromExecutableAddress(
                result.executableAddress())).callTarget());
    }
#else
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    return result;
}

static void repatchCall(CodeBlock* codeBlock, CodeLocationCall call, FunctionPtr newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITCode::FTLJIT) {
        VM& vm = *codeBlock->vm();
        FTL::Thunks& thunks = *vm.ftlThunks;
        FTL::SlowPathCallKey key = thunks.keyForSlowPathCallThunk(
            MacroAssemblerCodePtr::createFromExecutableAddress(
                MacroAssembler::readCallTarget(call).executableAddress()));
        key = key.withCallTarget(newCalleeFunction.executableAddress());
        newCalleeFunction = FunctionPtr(
            thunks.getSlowPathCallThunk(vm, key).code().executableAddress());
    }
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    MacroAssembler::repatchCall(call, newCalleeFunction);
}

static void repatchByIdSelfAccess(
    CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure,
    PropertyOffset offset, const FunctionPtr& slowPathFunction,
    bool compact)
{
    // Only optimize once!
    repatchCall(codeBlock, stubInfo.callReturnLocation, slowPathFunction);

    // Patch the structure check & the offset of the load.
    MacroAssembler::repatchInt32(
        stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall),
        bitwise_cast<int32_t>(structure->id()));
#if USE(JSVALUE64)
    if (compact)
        MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToLoadOrStore), offsetRelativeToBase(offset));
    else
        MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToLoadOrStore), offsetRelativeToBase(offset));
#elif USE(JSVALUE32_64)
    if (compact) {
        MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), offsetRelativeToBase(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), offsetRelativeToBase(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    } else {
        MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), offsetRelativeToBase(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), offsetRelativeToBase(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }
#endif
}

static void resetGetByIDCheckAndLoad(StructureStubInfo& stubInfo)
{
    CodeLocationDataLabel32 structureLabel = stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall);
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        MacroAssembler::revertJumpReplacementToPatchableBranch32WithPatch(
            MacroAssembler::startOfPatchableBranch32WithPatchOnAddress(structureLabel),
            MacroAssembler::Address(
                static_cast<MacroAssembler::RegisterID>(stubInfo.patch.baseGPR),
                JSCell::structureIDOffset()),
            static_cast<int32_t>(unusedPointer));
    }
    MacroAssembler::repatchInt32(structureLabel, static_cast<int32_t>(unusedPointer));
#if USE(JSVALUE64)
    MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToLoadOrStore), 0);
#else
    MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), 0);
    MacroAssembler::repatchCompact(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), 0);
#endif
}

static void resetPutByIDCheckAndLoad(StructureStubInfo& stubInfo)
{
    CodeLocationDataLabel32 structureLabel = stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall);
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        MacroAssembler::revertJumpReplacementToPatchableBranch32WithPatch(
            MacroAssembler::startOfPatchableBranch32WithPatchOnAddress(structureLabel),
            MacroAssembler::Address(
                static_cast<MacroAssembler::RegisterID>(stubInfo.patch.baseGPR),
                JSCell::structureIDOffset()),
            static_cast<int32_t>(unusedPointer));
    }
    MacroAssembler::repatchInt32(structureLabel, static_cast<int32_t>(unusedPointer));
#if USE(JSVALUE64)
    MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToLoadOrStore), 0);
#else
    MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), 0);
    MacroAssembler::repatchInt32(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), 0);
#endif
}

static void replaceWithJump(StructureStubInfo& stubInfo, const MacroAssemblerCodePtr target)
{
    RELEASE_ASSERT(target);
    
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        MacroAssembler::replaceWithJump(
            MacroAssembler::startOfPatchableBranch32WithPatchOnAddress(
                stubInfo.callReturnLocation.dataLabel32AtOffset(
                    -(intptr_t)stubInfo.patch.deltaCheckImmToCall)),
            CodeLocationLabel(target));
        return;
    }

    resetGetByIDCheckAndLoad(stubInfo);
    
    MacroAssembler::repatchJump(
        stubInfo.callReturnLocation.jumpAtOffset(
            stubInfo.patch.deltaCallToJump),
        CodeLocationLabel(target));
}

enum InlineCacheAction {
    GiveUpOnCache,
    RetryCacheLater,
    AttemptToCache
};

static InlineCacheAction actionForCell(VM& vm, JSCell* cell)
{
    Structure* structure = cell->structure(vm);

    TypeInfo typeInfo = structure->typeInfo();
    if (typeInfo.prohibitsPropertyCaching())
        return GiveUpOnCache;

    if (structure->isUncacheableDictionary()) {
        if (structure->hasBeenFlattenedBefore())
            return GiveUpOnCache;
        // Flattening could have changed the offset, so return early for another try.
        asObject(cell)->flattenDictionaryObject(vm);
        return RetryCacheLater;
    }
    
    if (!structure->propertyAccessesAreCacheable())
        return GiveUpOnCache;

    return AttemptToCache;
}

static bool forceICFailure(ExecState*)
{
    return Options::forceICFailure();
}

inline J_JITOperation_ESsiJI appropriateOptimizingGetByIdFunction(GetByIDKind kind)
{
    if (kind == GetByIDKind::Normal)
        return operationGetByIdOptimize;
    return operationTryGetByIdOptimize;
}

inline J_JITOperation_ESsiJI appropriateGenericGetByIdFunction(GetByIDKind kind)
{
    if (kind == GetByIDKind::Normal)
        return operationGetById;
    return operationTryGetById;
}

static InlineCacheAction tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    if (forceICFailure(exec))
        return GiveUpOnCache;
    
    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell())
        return GiveUpOnCache;

    CodeBlock* codeBlock = exec->codeBlock();
    VM& vm = exec->vm();

    std::unique_ptr<AccessCase> newCase;

    if (isJSArray(baseValue) && propertyName == exec->propertyNames().length)
        newCase = AccessCase::getLength(vm, codeBlock, AccessCase::ArrayLength);
    else if (isJSString(baseValue) && propertyName == exec->propertyNames().length)
        newCase = AccessCase::getLength(vm, codeBlock, AccessCase::StringLength);
    else {
        if (!slot.isCacheable() && !slot.isUnset())
            return GiveUpOnCache;

        ObjectPropertyConditionSet conditionSet;
        JSCell* baseCell = baseValue.asCell();
        Structure* structure = baseCell->structure(vm);

        bool loadTargetFromProxy = false;
        if (baseCell->type() == PureForwardingProxyType) {
            baseValue = jsCast<JSProxy*>(baseCell)->target();
            baseCell = baseValue.asCell();
            structure = baseCell->structure(vm);
            loadTargetFromProxy = true;
        }

        InlineCacheAction action = actionForCell(vm, baseCell);
        if (action != AttemptToCache)
            return action;
        
        // Optimize self access.
        if (stubInfo.cacheType == CacheType::Unset
            && slot.isCacheableValue()
            && slot.slotBase() == baseValue
            && !slot.watchpointSet()
            && isInlineOffset(slot.cachedOffset())
            && MacroAssembler::isCompactPtrAlignedAddressOffset(maxOffsetRelativeToBase(slot.cachedOffset()))
            && action == AttemptToCache
            && !structure->needImpurePropertyWatchpoint()
            && !loadTargetFromProxy) {
            structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
            repatchByIdSelfAccess(codeBlock, stubInfo, structure, slot.cachedOffset(), appropriateOptimizingGetByIdFunction(kind), true);
            stubInfo.initGetByIdSelf(codeBlock, structure, slot.cachedOffset());
            return RetryCacheLater;
        }

        PropertyOffset offset = slot.isUnset() ? invalidOffset : slot.cachedOffset();

        if (slot.isUnset() || slot.slotBase() != baseValue) {
            if (structure->typeInfo().prohibitsPropertyCaching() || structure->isDictionary())
                return GiveUpOnCache;
            
            if (slot.isUnset() && structure->typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence())
                return GiveUpOnCache;

            if (slot.isUnset()) {
                conditionSet = generateConditionsForPropertyMiss(
                    vm, codeBlock, exec, structure, propertyName.impl());
            } else {
                conditionSet = generateConditionsForPrototypePropertyHit(
                    vm, codeBlock, exec, structure, slot.slotBase(),
                    propertyName.impl());
            }
            
            if (!conditionSet.isValid())
                return GiveUpOnCache;

            offset = slot.isUnset() ? invalidOffset : conditionSet.slotBaseCondition().offset();
        }

        JSFunction* getter = nullptr;
        if (slot.isCacheableGetter())
            getter = jsDynamicCast<JSFunction*>(slot.getterSetter()->getter());

        if (kind == GetByIDKind::Pure) {
            AccessCase::AccessType type;
            if (slot.isCacheableValue())
                type = AccessCase::Load;
            else if (slot.isUnset())
                type = AccessCase::Miss;
            else if (slot.isCacheableGetter())
                type = AccessCase::GetGetter;
            else
                RELEASE_ASSERT_NOT_REACHED();

            newCase = AccessCase::tryGet(vm, codeBlock, type, offset, structure, conditionSet, loadTargetFromProxy, slot.watchpointSet());
        } else if (!loadTargetFromProxy && getter && AccessCase::canEmitIntrinsicGetter(getter, structure))
            newCase = AccessCase::getIntrinsic(vm, codeBlock, getter, slot.cachedOffset(), structure, conditionSet);
        else {
            AccessCase::AccessType type;
            if (slot.isCacheableValue())
                type = AccessCase::Load;
            else if (slot.isUnset())
                type = AccessCase::Miss;
            else if (slot.isCacheableGetter())
                type = AccessCase::Getter;
            else if (slot.attributes() & CustomAccessor)
                type = AccessCase::CustomAccessorGetter;
            else
                type = AccessCase::CustomValueGetter;

            newCase = AccessCase::get(
                vm, codeBlock, type, offset, structure, conditionSet, loadTargetFromProxy,
                slot.watchpointSet(), slot.isCacheableCustom() ? slot.customGetter() : nullptr,
                slot.isCacheableCustom() ? slot.slotBase() : nullptr);
        }
    }

    AccessGenerationResult result = stubInfo.addAccessCase(codeBlock, propertyName, WTFMove(newCase));

    if (result.gaveUp())
        return GiveUpOnCache;
    if (result.madeNoChanges())
        return RetryCacheLater;
    
    RELEASE_ASSERT(result.code());
    replaceWithJump(stubInfo, result.code());
    
    return RetryCacheLater;
}

void repatchGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    if (tryCacheGetByID(exec, baseValue, propertyName, slot, stubInfo, kind) == GiveUpOnCache)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericGetByIdFunction(kind));
}

static V_JITOperation_ESsiJJI appropriateGenericPutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
{
    if (slot.isStrictMode()) {
        if (putKind == Direct)
            return operationPutByIdDirectStrict;
        return operationPutByIdStrict;
    }
    if (putKind == Direct)
        return operationPutByIdDirectNonStrict;
    return operationPutByIdNonStrict;
}

static V_JITOperation_ESsiJJI appropriateOptimizingPutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
{
    if (slot.isStrictMode()) {
        if (putKind == Direct)
            return operationPutByIdDirectStrictOptimize;
        return operationPutByIdStrictOptimize;
    }
    if (putKind == Direct)
        return operationPutByIdDirectNonStrictOptimize;
    return operationPutByIdNonStrictOptimize;
}

static InlineCacheAction tryCachePutByID(ExecState* exec, JSValue baseValue, Structure* structure, const Identifier& ident, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    if (forceICFailure(exec))
        return GiveUpOnCache;
    
    CodeBlock* codeBlock = exec->codeBlock();
    VM& vm = exec->vm();

    if (!baseValue.isCell())
        return GiveUpOnCache;
    
    if (!slot.isCacheablePut() && !slot.isCacheableCustom() && !slot.isCacheableSetter())
        return GiveUpOnCache;

    if (!structure->propertyAccessesAreCacheable())
        return GiveUpOnCache;

    std::unique_ptr<AccessCase> newCase;

    if (slot.base() == baseValue && slot.isCacheablePut()) {
        if (slot.type() == PutPropertySlot::ExistingProperty) {
            structure->didCachePropertyReplacement(vm, slot.cachedOffset());
        
            if (stubInfo.cacheType == CacheType::Unset
                && isInlineOffset(slot.cachedOffset())
                && MacroAssembler::isPtrAlignedAddressOffset(maxOffsetRelativeToBase(slot.cachedOffset()))
                && !structure->needImpurePropertyWatchpoint()
                && !structure->inferredTypeFor(ident.impl())) {

                repatchByIdSelfAccess(
                    codeBlock, stubInfo, structure, slot.cachedOffset(),
                    appropriateOptimizingPutByIdFunction(slot, putKind), false);
                stubInfo.initPutByIdReplace(codeBlock, structure, slot.cachedOffset());
                return RetryCacheLater;
            }

            newCase = AccessCase::replace(vm, codeBlock, structure, slot.cachedOffset());
        } else {
            ASSERT(slot.type() == PutPropertySlot::NewProperty);

            if (!structure->isObject() || structure->isDictionary())
                return GiveUpOnCache;

            PropertyOffset offset;
            Structure* newStructure =
                Structure::addPropertyTransitionToExistingStructureConcurrently(
                    structure, ident.impl(), 0, offset);
            if (!newStructure || !newStructure->propertyAccessesAreCacheable())
                return GiveUpOnCache;

            ASSERT(newStructure->previousID() == structure);
            ASSERT(!newStructure->isDictionary());
            ASSERT(newStructure->isObject());
            
            ObjectPropertyConditionSet conditionSet;
            if (putKind == NotDirect) {
                conditionSet =
                    generateConditionsForPropertySetterMiss(
                        vm, codeBlock, exec, newStructure, ident.impl());
                if (!conditionSet.isValid())
                    return GiveUpOnCache;
            }

            newCase = AccessCase::transition(vm, codeBlock, structure, newStructure, offset, conditionSet);
        }
    } else if (slot.isCacheableCustom() || slot.isCacheableSetter()) {
        if (slot.isCacheableCustom()) {
            ObjectPropertyConditionSet conditionSet;

            if (slot.base() != baseValue) {
                conditionSet =
                    generateConditionsForPrototypePropertyHitCustom(
                        vm, codeBlock, exec, structure, slot.base(), ident.impl());
                if (!conditionSet.isValid())
                    return GiveUpOnCache;
            }

            newCase = AccessCase::setter(
                vm, codeBlock, slot.isCustomAccessor() ? AccessCase::CustomAccessorSetter : AccessCase::CustomValueSetter, structure, invalidOffset, conditionSet,
                slot.customSetter(), slot.base());
        } else {
            ObjectPropertyConditionSet conditionSet;
            PropertyOffset offset;

            if (slot.base() != baseValue) {
                conditionSet =
                    generateConditionsForPrototypePropertyHit(
                        vm, codeBlock, exec, structure, slot.base(), ident.impl());
                if (!conditionSet.isValid())
                    return GiveUpOnCache;
                offset = conditionSet.slotBaseCondition().offset();
            } else
                offset = slot.cachedOffset();

            newCase = AccessCase::setter(
                vm, codeBlock, AccessCase::Setter, structure, offset, conditionSet);
        }
    }

    AccessGenerationResult result = stubInfo.addAccessCase(codeBlock, ident, WTFMove(newCase));
    
    if (result.gaveUp())
        return GiveUpOnCache;
    if (result.madeNoChanges())
        return RetryCacheLater;

    RELEASE_ASSERT(result.code());
    resetPutByIDCheckAndLoad(stubInfo);
    MacroAssembler::repatchJump(
        stubInfo.callReturnLocation.jumpAtOffset(
            stubInfo.patch.deltaCallToJump),
        CodeLocationLabel(result.code()));
    
    return RetryCacheLater;
}

void repatchPutByID(ExecState* exec, JSValue baseValue, Structure* structure, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    if (tryCachePutByID(exec, baseValue, structure, propertyName, slot, stubInfo, putKind) == GiveUpOnCache)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

static InlineCacheAction tryRepatchIn(
    ExecState* exec, JSCell* base, const Identifier& ident, bool wasFound,
    const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (forceICFailure(exec))
        return GiveUpOnCache;
    
    if (!base->structure()->propertyAccessesAreCacheable() || (!wasFound && !base->structure()->propertyAccessesAreCacheableForAbsence()))
        return GiveUpOnCache;
    
    if (wasFound) {
        if (!slot.isCacheable())
            return GiveUpOnCache;
    }
    
    CodeBlock* codeBlock = exec->codeBlock();
    VM& vm = exec->vm();
    Structure* structure = base->structure(vm);
    
    ObjectPropertyConditionSet conditionSet;
    if (wasFound) {
        if (slot.slotBase() != base) {
            conditionSet = generateConditionsForPrototypePropertyHit(
                vm, codeBlock, exec, structure, slot.slotBase(), ident.impl());
        }
    } else {
        conditionSet = generateConditionsForPropertyMiss(
            vm, codeBlock, exec, structure, ident.impl());
    }
    if (!conditionSet.isValid())
        return GiveUpOnCache;

    std::unique_ptr<AccessCase> newCase = AccessCase::in(
        vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, structure, conditionSet);

    AccessGenerationResult result = stubInfo.addAccessCase(codeBlock, ident, WTFMove(newCase));
    if (result.gaveUp())
        return GiveUpOnCache;
    if (result.madeNoChanges())
        return RetryCacheLater;

    RELEASE_ASSERT(result.code());
    MacroAssembler::repatchJump(
        stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump),
        CodeLocationLabel(result.code()));
    
    return RetryCacheLater;
}

void repatchIn(
    ExecState* exec, JSCell* base, const Identifier& ident, bool wasFound,
    const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (tryRepatchIn(exec, base, ident, wasFound, slot, stubInfo) == GiveUpOnCache)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationIn);
}

static void linkSlowFor(VM*, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef codeRef)
{
    MacroAssembler::repatchNearCall(callLinkInfo.callReturnLocation(), CodeLocationLabel(codeRef.code()));
}

static void linkSlowFor(VM* vm, CallLinkInfo& callLinkInfo, ThunkGenerator generator)
{
    linkSlowFor(vm, callLinkInfo, vm->getCTIStub(generator));
}

static void linkSlowFor(VM* vm, CallLinkInfo& callLinkInfo)
{
    MacroAssemblerCodeRef virtualThunk = virtualThunkFor(vm, callLinkInfo);
    linkSlowFor(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(createJITStubRoutine(virtualThunk, *vm, nullptr, true));
}

void linkFor(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    JSFunction* callee, MacroAssemblerCodePtr codePtr)
{
    ASSERT(!callLinkInfo.stub());
    
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();

    VM* vm = callerCodeBlock->vm();
    
    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setCallee(exec->callerFrame()->vm(), callLinkInfo.hotPathBegin(), callerCodeBlock, callee);
    callLinkInfo.setLastSeenCallee(exec->callerFrame()->vm(), callerCodeBlock, callee);
    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", *callerCodeBlock, " at ", callLinkInfo.codeOrigin(), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");
    MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), CodeLocationLabel(codePtr));
    
    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(exec->callerFrame(), &callLinkInfo);
    
    if (callLinkInfo.specializationKind() == CodeForCall && callLinkInfo.allowStubs()) {
        linkSlowFor(vm, callLinkInfo, linkPolymorphicCallThunkGenerator);
        return;
    }
    
    linkSlowFor(vm, callLinkInfo);
}

void linkSlowFor(
    ExecState* exec, CallLinkInfo& callLinkInfo)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();
    
    linkSlowFor(vm, callLinkInfo);
}

static void revertCall(VM* vm, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef codeRef)
{
    MacroAssembler::revertJumpReplacementToBranchPtrWithPatch(
        MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
        static_cast<MacroAssembler::RegisterID>(callLinkInfo.calleeGPR()), 0);
    linkSlowFor(vm, callLinkInfo, codeRef);
    callLinkInfo.clearSeen();
    callLinkInfo.clearCallee();
    callLinkInfo.clearStub();
    callLinkInfo.clearSlowStub();
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void unlinkFor(VM& vm, CallLinkInfo& callLinkInfo)
{
    if (Options::dumpDisassembly())
        dataLog("Unlinking call from ", callLinkInfo.callReturnLocation(), "\n");
    
    revertCall(&vm, callLinkInfo, vm.getCTIStub(linkCallThunkGenerator));
}

void linkVirtualFor(
    ExecState* exec, CallLinkInfo& callLinkInfo)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();

    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking virtual call at ", *callerCodeBlock, " ", exec->callerFrame()->codeOrigin(), "\n");
    
    MacroAssemblerCodeRef virtualThunk = virtualThunkFor(vm, callLinkInfo);
    revertCall(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(createJITStubRoutine(virtualThunk, *vm, nullptr, true));
}

namespace {
struct CallToCodePtr {
    CCallHelpers::Call call;
    MacroAssemblerCodePtr codePtr;
};
} // annonymous namespace

void linkPolymorphicCall(
    ExecState* exec, CallLinkInfo& callLinkInfo, CallVariant newVariant)
{
    RELEASE_ASSERT(callLinkInfo.allowStubs());
    
    // Currently we can't do anything for non-function callees.
    // https://bugs.webkit.org/show_bug.cgi?id=140685
    if (!newVariant || !newVariant.executable()) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }
    
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();
    
    CallVariantList list;
    if (PolymorphicCallStubRoutine* stub = callLinkInfo.stub())
        list = stub->variants();
    else if (JSFunction* oldCallee = callLinkInfo.callee())
        list = CallVariantList{ CallVariant(oldCallee) };
    
    list = variantListWithVariant(list, newVariant);

    // If there are any closure calls then it makes sense to treat all of them as closure calls.
    // This makes switching on callee cheaper. It also produces profiling that's easier on the DFG;
    // the DFG doesn't really want to deal with a combination of closure and non-closure callees.
    bool isClosureCall = false;
    for (CallVariant variant : list)  {
        if (variant.isClosureCall()) {
            list = despecifiedVariantList(list);
            isClosureCall = true;
            break;
        }
    }
    
    if (isClosureCall)
        callLinkInfo.setHasSeenClosure();
    
    Vector<PolymorphicCallCase> callCases;
    
    // Figure out what our cases are.
    for (CallVariant variant : list) {
        CodeBlock* codeBlock;
        if (variant.executable()->isHostFunction())
            codeBlock = nullptr;
        else {
            ExecutableBase* executable = variant.executable();
#if ENABLE(WEBASSEMBLY)
            if (executable->isWebAssemblyExecutable())
                codeBlock = jsCast<WebAssemblyExecutable*>(executable)->codeBlockForCall();
            else
#endif
                codeBlock = jsCast<FunctionExecutable*>(executable)->codeBlockForCall();
            // If we cannot handle a callee, either because we don't have a CodeBlock or because arity mismatch,
            // assume that it's better for this whole thing to be a virtual call.
            if (!codeBlock || exec->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo.isVarargs()) {
                linkVirtualFor(exec, callLinkInfo);
                return;
            }
        }
        
        callCases.append(PolymorphicCallCase(variant, codeBlock));
    }
    
    // If we are over the limit, just use a normal virtual call.
    unsigned maxPolymorphicCallVariantListSize;
    if (callerCodeBlock->jitType() == JITCode::topTierJIT())
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForTopTier();
    else
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSize();
    if (list.size() > maxPolymorphicCallVariantListSize) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }
    
    GPRReg calleeGPR = static_cast<GPRReg>(callLinkInfo.calleeGPR());
    
    CCallHelpers stubJit(vm, callerCodeBlock);
    
    CCallHelpers::JumpList slowPath;
    
    std::unique_ptr<CallFrameShuffler> frameShuffler;
    if (callLinkInfo.frameShuffleData()) {
        ASSERT(callLinkInfo.isTailCall());
        frameShuffler = std::make_unique<CallFrameShuffler>(stubJit, *callLinkInfo.frameShuffleData());
#if USE(JSVALUE32_64)
        // We would have already checked that the callee is a cell, and we can
        // use the additional register this buys us.
        frameShuffler->assumeCalleeIsCell();
#endif
        frameShuffler->lockGPR(calleeGPR);
    }
    GPRReg comparisonValueGPR;
    
    if (isClosureCall) {
        GPRReg scratchGPR;
        if (frameShuffler)
            scratchGPR = frameShuffler->acquireGPR();
        else
            scratchGPR = AssemblyHelpers::selectScratchGPR(calleeGPR);
        // Verify that we have a function and stash the executable in scratchGPR.

#if USE(JSVALUE64)
        // We can't rely on tagMaskRegister being set, so we do this the hard
        // way.
        stubJit.move(MacroAssembler::TrustedImm64(TagMask), scratchGPR);
        slowPath.append(stubJit.branchTest64(CCallHelpers::NonZero, calleeGPR, scratchGPR));
#else
        // We would have already checked that the callee is a cell.
#endif
    
        slowPath.append(
            stubJit.branch8(
                CCallHelpers::NotEqual,
                CCallHelpers::Address(calleeGPR, JSCell::typeInfoTypeOffset()),
                CCallHelpers::TrustedImm32(JSFunctionType)));
    
        stubJit.loadPtr(
            CCallHelpers::Address(calleeGPR, JSFunction::offsetOfExecutable()),
            scratchGPR);
        
        comparisonValueGPR = scratchGPR;
    } else
        comparisonValueGPR = calleeGPR;
    
    Vector<int64_t> caseValues(callCases.size());
    Vector<CallToCodePtr> calls(callCases.size());
    std::unique_ptr<uint32_t[]> fastCounts;
    
    if (callerCodeBlock->jitType() != JITCode::topTierJIT())
        fastCounts = std::make_unique<uint32_t[]>(callCases.size());
    
    for (size_t i = 0; i < callCases.size(); ++i) {
        if (fastCounts)
            fastCounts[i] = 0;
        
        CallVariant variant = callCases[i].variant();
        int64_t newCaseValue;
        if (isClosureCall)
            newCaseValue = bitwise_cast<intptr_t>(variant.executable());
        else
            newCaseValue = bitwise_cast<intptr_t>(variant.function());
        
        if (!ASSERT_DISABLED) {
            for (size_t j = 0; j < i; ++j) {
                if (caseValues[j] != newCaseValue)
                    continue;

                dataLog("ERROR: Attempt to add duplicate case value.\n");
                dataLog("Existing case values: ");
                CommaPrinter comma;
                for (size_t k = 0; k < i; ++k)
                    dataLog(comma, caseValues[k]);
                dataLog("\n");
                dataLog("Attempting to add: ", newCaseValue, "\n");
                dataLog("Variant list: ", listDump(callCases), "\n");
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        
        caseValues[i] = newCaseValue;
    }
    
    GPRReg fastCountsBaseGPR;
    if (frameShuffler)
        fastCountsBaseGPR = frameShuffler->acquireGPR();
    else {
        fastCountsBaseGPR =
            AssemblyHelpers::selectScratchGPR(calleeGPR, comparisonValueGPR, GPRInfo::regT3);
    }
    stubJit.move(CCallHelpers::TrustedImmPtr(fastCounts.get()), fastCountsBaseGPR);
    if (!frameShuffler && callLinkInfo.isTailCall())
        stubJit.emitRestoreCalleeSaves();
    BinarySwitch binarySwitch(comparisonValueGPR, caseValues, BinarySwitch::IntPtr);
    CCallHelpers::JumpList done;
    while (binarySwitch.advance(stubJit)) {
        size_t caseIndex = binarySwitch.caseIndex();
        
        CallVariant variant = callCases[caseIndex].variant();
        
        ASSERT(variant.executable()->hasJITCodeForCall());
        MacroAssemblerCodePtr codePtr =
            variant.executable()->generatedJITCodeForCall()->addressForCall(ArityCheckNotRequired);
        
        if (fastCounts) {
            stubJit.add32(
                CCallHelpers::TrustedImm32(1),
                CCallHelpers::Address(fastCountsBaseGPR, caseIndex * sizeof(uint32_t)));
        }
        if (frameShuffler) {
            CallFrameShuffler(stubJit, frameShuffler->snapshot()).prepareForTailCall();
            calls[caseIndex].call = stubJit.nearTailCall();
        } else if (callLinkInfo.isTailCall()) {
            stubJit.prepareForTailCallSlow();
            calls[caseIndex].call = stubJit.nearTailCall();
        } else
            calls[caseIndex].call = stubJit.nearCall();
        calls[caseIndex].codePtr = codePtr;
        done.append(stubJit.jump());
    }
    
    slowPath.link(&stubJit);
    binarySwitch.fallThrough().link(&stubJit);

    if (frameShuffler) {
        frameShuffler->releaseGPR(calleeGPR);
        frameShuffler->releaseGPR(comparisonValueGPR);
        frameShuffler->releaseGPR(fastCountsBaseGPR);
#if USE(JSVALUE32_64)
        frameShuffler->setCalleeJSValueRegs(JSValueRegs(GPRInfo::regT1, GPRInfo::regT0));
#else
        frameShuffler->setCalleeJSValueRegs(JSValueRegs(GPRInfo::regT0));
#endif
        frameShuffler->prepareForSlowPath();
    } else {
        stubJit.move(calleeGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
        stubJit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
    }
    stubJit.move(CCallHelpers::TrustedImmPtr(&callLinkInfo), GPRInfo::regT2);
    stubJit.move(CCallHelpers::TrustedImmPtr(callLinkInfo.callReturnLocation().executableAddress()), GPRInfo::regT4);
    
    stubJit.restoreReturnAddressBeforeReturn(GPRInfo::regT4);
    AssemblyHelpers::Jump slow = stubJit.jump();
        
    LinkBuffer patchBuffer(*vm, stubJit, callerCodeBlock, JITCompilationCanFail);
    if (patchBuffer.didFailToAllocate()) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }
    
    RELEASE_ASSERT(callCases.size() == calls.size());
    for (CallToCodePtr callToCodePtr : calls) {
        patchBuffer.link(
            callToCodePtr.call, FunctionPtr(callToCodePtr.codePtr.executableAddress()));
    }
    if (JITCode::isOptimizingJIT(callerCodeBlock->jitType()))
        patchBuffer.link(done, callLinkInfo.callReturnLocation().labelAtOffset(0));
    else
        patchBuffer.link(done, callLinkInfo.hotPathOther().labelAtOffset(0));
    patchBuffer.link(slow, CodeLocationLabel(vm->getCTIStub(linkPolymorphicCallThunkGenerator).code()));
    
    RefPtr<PolymorphicCallStubRoutine> stubRoutine = adoptRef(new PolymorphicCallStubRoutine(
        FINALIZE_CODE_FOR(
            callerCodeBlock, patchBuffer,
            ("Polymorphic call stub for %s, return point %p, targets %s",
                toCString(*callerCodeBlock).data(), callLinkInfo.callReturnLocation().labelAtOffset(0).executableAddress(),
                toCString(listDump(callCases)).data())),
        *vm, callerCodeBlock, exec->callerFrame(), callLinkInfo, callCases,
        WTFMove(fastCounts)));
    
    MacroAssembler::replaceWithJump(
        MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
        CodeLocationLabel(stubRoutine->code().code()));
    // The original slow path is unreachable on 64-bits, but still
    // reachable on 32-bits since a non-cell callee will always
    // trigger the slow path
    linkSlowFor(vm, callLinkInfo);
    
    // If there had been a previous stub routine, that one will die as soon as the GC runs and sees
    // that it's no longer on stack.
    callLinkInfo.setStub(stubRoutine.release());
    
    // The call link info no longer has a call cache apart from the jump to the polymorphic call
    // stub.
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void resetGetByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    repatchCall(codeBlock, stubInfo.callReturnLocation, appropriateOptimizingGetByIdFunction(kind));
    resetGetByIDCheckAndLoad(stubInfo);
    MacroAssembler::repatchJump(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

void resetPutByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    V_JITOperation_ESsiJJI unoptimizedFunction = bitwise_cast<V_JITOperation_ESsiJJI>(readCallTarget(codeBlock, stubInfo.callReturnLocation).executableAddress());
    V_JITOperation_ESsiJJI optimizedFunction;
    if (unoptimizedFunction == operationPutByIdStrict || unoptimizedFunction == operationPutByIdStrictOptimize)
        optimizedFunction = operationPutByIdStrictOptimize;
    else if (unoptimizedFunction == operationPutByIdNonStrict || unoptimizedFunction == operationPutByIdNonStrictOptimize)
        optimizedFunction = operationPutByIdNonStrictOptimize;
    else if (unoptimizedFunction == operationPutByIdDirectStrict || unoptimizedFunction == operationPutByIdDirectStrictOptimize)
        optimizedFunction = operationPutByIdDirectStrictOptimize;
    else {
        ASSERT(unoptimizedFunction == operationPutByIdDirectNonStrict || unoptimizedFunction == operationPutByIdDirectNonStrictOptimize);
        optimizedFunction = operationPutByIdDirectNonStrictOptimize;
    }
    repatchCall(codeBlock, stubInfo.callReturnLocation, optimizedFunction);
    resetPutByIDCheckAndLoad(stubInfo);
    MacroAssembler::repatchJump(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

void resetIn(CodeBlock*, StructureStubInfo& stubInfo)
{
    MacroAssembler::repatchJump(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

} // namespace JSC

#endif
