/*
 * Copyright (C) 2011, 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#include "CCallHelpers.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "FTLThunks.h"
#include "GCAwareJITStubRoutine.h"
#include "JIT.h"
#include "JITInlines.h"
#include "LinkBuffer.h"
#include "JSCInlines.h"
#include "PolymorphicPutByIdList.h"
#include "RepatchBuffer.h"
#include "ScratchRegisterAllocator.h"
#include "StackAlignment.h"
#include "StructureRareDataInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "ThunkGenerators.h"
#include <wtf/StringPrintStream.h>

namespace JSC {

// Beware: in this code, it is not safe to assume anything about the following registers
// that would ordinarily have well-known values:
// - tagTypeNumberRegister
// - tagMaskRegister

static FunctionPtr readCallTarget(RepatchBuffer& repatchBuffer, CodeLocationCall call)
{
    FunctionPtr result = MacroAssembler::readCallTarget(call);
#if ENABLE(FTL_JIT)
    CodeBlock* codeBlock = repatchBuffer.codeBlock();
    if (codeBlock->jitType() == JITCode::FTLJIT) {
        return FunctionPtr(codeBlock->vm()->ftlThunks->keyForSlowPathCallThunk(
            MacroAssemblerCodePtr::createFromExecutableAddress(
                result.executableAddress())).callTarget());
    }
#else
    UNUSED_PARAM(repatchBuffer);
#endif // ENABLE(FTL_JIT)
    return result;
}

static void repatchCall(RepatchBuffer& repatchBuffer, CodeLocationCall call, FunctionPtr newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    CodeBlock* codeBlock = repatchBuffer.codeBlock();
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
#endif // ENABLE(FTL_JIT)
    repatchBuffer.relink(call, newCalleeFunction);
}

static void repatchCall(CodeBlock* codeblock, CodeLocationCall call, FunctionPtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchCall(repatchBuffer, call, newCalleeFunction);
}

static void repatchByIdSelfAccess(VM& vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure, const Identifier& propertyName, PropertyOffset offset,
    const FunctionPtr &slowPathFunction, bool compact)
{
    if (structure->typeInfo().newImpurePropertyFiresWatchpoints())
        vm.registerWatchpointForImpureProperty(propertyName, stubInfo.addWatchpoint(codeBlock));

    RepatchBuffer repatchBuffer(codeBlock);

    // Only optimize once!
    repatchCall(repatchBuffer, stubInfo.callReturnLocation, slowPathFunction);

    // Patch the structure check & the offset of the load.
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall), bitwise_cast<int32_t>(structure->id()));
    repatchBuffer.setLoadInstructionIsActive(stubInfo.callReturnLocation.convertibleLoadAtOffset(stubInfo.patch.deltaCallToStorageLoad), isOutOfLineOffset(offset));
#if USE(JSVALUE64)
    if (compact)
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToLoadOrStore), offsetRelativeToPatchedStorage(offset));
    else
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToLoadOrStore), offsetRelativeToPatchedStorage(offset));
#elif USE(JSVALUE32_64)
    if (compact) {
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), offsetRelativeToPatchedStorage(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), offsetRelativeToPatchedStorage(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    } else {
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), offsetRelativeToPatchedStorage(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), offsetRelativeToPatchedStorage(offset) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }
#endif
}

static void addStructureTransitionCheck(
    JSCell* object, Structure* structure, CodeBlock* codeBlock, StructureStubInfo& stubInfo,
    MacroAssembler& jit, MacroAssembler::JumpList& failureCases, GPRReg scratchGPR)
{
    if (object->structure() == structure && structure->transitionWatchpointSetIsStillValid()) {
        structure->addTransitionWatchpoint(stubInfo.addWatchpoint(codeBlock));
#if !ASSERT_DISABLED
        // If we execute this code, the object must have the structure we expect. Assert
        // this in debug modes.
        jit.move(MacroAssembler::TrustedImmPtr(object), scratchGPR);
        MacroAssembler::Jump ok = branchStructure(jit,
            MacroAssembler::Equal,
            MacroAssembler::Address(scratchGPR, JSCell::structureIDOffset()),
            structure);
        jit.breakpoint();
        ok.link(&jit);
#endif
        return;
    }
    
    jit.move(MacroAssembler::TrustedImmPtr(object), scratchGPR);
    failureCases.append(
        branchStructure(jit,
            MacroAssembler::NotEqual,
            MacroAssembler::Address(scratchGPR, JSCell::structureIDOffset()),
            structure));
}

static void addStructureTransitionCheck(
    JSValue prototype, CodeBlock* codeBlock, StructureStubInfo& stubInfo,
    MacroAssembler& jit, MacroAssembler::JumpList& failureCases, GPRReg scratchGPR)
{
    if (prototype.isNull())
        return;
    
    ASSERT(prototype.isCell());
    
    addStructureTransitionCheck(
        prototype.asCell(), prototype.asCell()->structure(), codeBlock, stubInfo, jit,
        failureCases, scratchGPR);
}

static void replaceWithJump(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo, const MacroAssemblerCodePtr target)
{
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        repatchBuffer.replaceWithJump(
            RepatchBuffer::startOfPatchableBranch32WithPatchOnAddress(
                stubInfo.callReturnLocation.dataLabel32AtOffset(
                    -(intptr_t)stubInfo.patch.deltaCheckImmToCall)),
            CodeLocationLabel(target));
        return;
    }
    
    repatchBuffer.relink(
        stubInfo.callReturnLocation.jumpAtOffset(
            stubInfo.patch.deltaCallToJump),
        CodeLocationLabel(target));
}

static void emitRestoreScratch(MacroAssembler& stubJit, bool needToRestoreScratch, GPRReg scratchGPR, MacroAssembler::Jump& success, MacroAssembler::Jump& fail, MacroAssembler::JumpList failureCases)
{
    if (needToRestoreScratch) {
        stubJit.popToRestore(scratchGPR);
        
        success = stubJit.jump();
        
        // link failure cases here, so we can pop scratchGPR, and then jump back.
        failureCases.link(&stubJit);
        
        stubJit.popToRestore(scratchGPR);
        
        fail = stubJit.jump();
        return;
    }
    
    success = stubJit.jump();
}

static void linkRestoreScratch(LinkBuffer& patchBuffer, bool needToRestoreScratch, MacroAssembler::Jump success, MacroAssembler::Jump fail, MacroAssembler::JumpList failureCases, CodeLocationLabel successLabel, CodeLocationLabel slowCaseBegin)
{
    patchBuffer.link(success, successLabel);
        
    if (needToRestoreScratch) {
        patchBuffer.link(fail, slowCaseBegin);
        return;
    }
    
    // link failure cases directly back to normal path
    patchBuffer.link(failureCases, slowCaseBegin);
}

static void linkRestoreScratch(LinkBuffer& patchBuffer, bool needToRestoreScratch, StructureStubInfo& stubInfo, MacroAssembler::Jump success, MacroAssembler::Jump fail, MacroAssembler::JumpList failureCases)
{
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

enum ProtoChainGenerationResult {
    ProtoChainGenerationFailed,
    ProtoChainGenerationSucceeded
};

static ProtoChainGenerationResult generateProtoChainAccessStub(ExecState*, const PropertySlot&, const Identifier&, StructureStubInfo&, StructureChain*, size_t, PropertyOffset, Structure*, CodeLocationLabel, CodeLocationLabel, RefPtr<JITStubRoutine>&) WARN_UNUSED_RETURN;
static ProtoChainGenerationResult generateProtoChainAccessStub(ExecState* exec, const PropertySlot& slot, const Identifier& propertyName, StructureStubInfo& stubInfo, StructureChain* chain, size_t count, PropertyOffset offset, Structure* structure, CodeLocationLabel successLabel, CodeLocationLabel slowCaseLabel, RefPtr<JITStubRoutine>& stubRoutine)
{
    VM* vm = &exec->vm();
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
    GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
    GPRReg scratchGPR = TempRegisterSet(stubInfo.patch.usedRegisters).getFreeGPR();
    bool needToRestoreScratch = scratchGPR == InvalidGPRReg;
    if (needToRestoreScratch && !slot.isCacheableValue())
        return ProtoChainGenerationFailed;
    
    CCallHelpers stubJit(&exec->vm(), exec->codeBlock());
    if (needToRestoreScratch) {
#if USE(JSVALUE64)
        scratchGPR = AssemblyHelpers::selectScratchGPR(baseGPR, resultGPR);
#else
        scratchGPR = AssemblyHelpers::selectScratchGPR(baseGPR, resultGPR, resultTagGPR);
#endif
        stubJit.pushToSave(scratchGPR);
        needToRestoreScratch = true;
    }
    
    MacroAssembler::JumpList failureCases;
    
    failureCases.append(branchStructure(stubJit,
        MacroAssembler::NotEqual, 
        MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()), 
        structure));

    CodeBlock* codeBlock = exec->codeBlock();
    if (structure->typeInfo().newImpurePropertyFiresWatchpoints())
        vm->registerWatchpointForImpureProperty(propertyName, stubInfo.addWatchpoint(codeBlock));

    Structure* currStructure = structure;
    WriteBarrier<Structure>* it = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i, ++it) {
        protoObject = asObject(currStructure->prototypeForLookup(exec));
        Structure* protoStructure = protoObject->structure();
        if (protoStructure->typeInfo().newImpurePropertyFiresWatchpoints())
            vm->registerWatchpointForImpureProperty(propertyName, stubInfo.addWatchpoint(codeBlock));
        addStructureTransitionCheck(
            protoObject, protoStructure, codeBlock, stubInfo, stubJit,
            failureCases, scratchGPR);
        currStructure = it->get();
    }
    
    bool isAccessor = slot.isCacheableGetter() || slot.isCacheableCustom();
    if (isAccessor)
        stubJit.move(baseGPR, scratchGPR);

    if (!slot.isCacheableCustom()) {
        if (isInlineOffset(offset)) {
#if USE(JSVALUE64)
            stubJit.load64(protoObject->locationForOffset(offset), resultGPR);
#elif USE(JSVALUE32_64)
            stubJit.move(MacroAssembler::TrustedImmPtr(protoObject->locationForOffset(offset)), resultGPR);
            stubJit.load32(MacroAssembler::Address(resultGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
            stubJit.load32(MacroAssembler::Address(resultGPR, OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
#endif
        } else {
            stubJit.loadPtr(protoObject->butterflyAddress(), resultGPR);
#if USE(JSVALUE64)
            stubJit.load64(MacroAssembler::Address(resultGPR, offsetInButterfly(offset) * sizeof(WriteBarrier<Unknown>)), resultGPR);
#elif USE(JSVALUE32_64)
            stubJit.load32(MacroAssembler::Address(resultGPR, offsetInButterfly(offset) * sizeof(WriteBarrier<Unknown>) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
            stubJit.load32(MacroAssembler::Address(resultGPR, offsetInButterfly(offset) * sizeof(WriteBarrier<Unknown>) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
#endif
        }
    }

    MacroAssembler::Call operationCall;
    MacroAssembler::Call handlerCall;
    FunctionPtr operationFunction;
    MacroAssembler::Jump success, fail;
    if (isAccessor) {
        if (slot.isCacheableGetter()) {
            stubJit.setupArgumentsWithExecState(scratchGPR, resultGPR);
            operationFunction = operationCallGetter;
        } else {
            // EncodedJSValue (*GetValueFunc)(ExecState*, JSObject* slotBase, EncodedJSValue thisValue, PropertyName);
#if USE(JSVALUE64)
            stubJit.setupArgumentsWithExecState(MacroAssembler::TrustedImmPtr(protoObject), scratchGPR, MacroAssembler::TrustedImmPtr(propertyName.impl()));
#else
            stubJit.setupArgumentsWithExecState(MacroAssembler::TrustedImmPtr(protoObject), scratchGPR, MacroAssembler::TrustedImm32(JSValue::CellTag), MacroAssembler::TrustedImmPtr(propertyName.impl()));
#endif
            operationFunction = FunctionPtr(slot.customGetter());
        }

        // Need to make sure that whenever this call is made in the future, we remember the
        // place that we made it from. It just so happens to be the place that we are at
        // right now!
        stubJit.store32(MacroAssembler::TrustedImm32(exec->locationAsRawBits()),
            CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
        stubJit.storePtr(GPRInfo::callFrameRegister, &vm->topCallFrame);

        operationCall = stubJit.call();
#if USE(JSVALUE64)
        stubJit.move(GPRInfo::returnValueGPR, resultGPR);
#else
        stubJit.setupResults(resultGPR, resultTagGPR);
#endif
        MacroAssembler::Jump noException = stubJit.emitExceptionCheck(CCallHelpers::InvertedExceptionCheck);

        stubJit.setupArguments(CCallHelpers::TrustedImmPtr(vm), GPRInfo::callFrameRegister);
        handlerCall = stubJit.call();
        stubJit.jumpToExceptionHandler();
        
        noException.link(&stubJit);
    }
    emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
    
    LinkBuffer patchBuffer(*vm, &stubJit, exec->codeBlock());
    
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, successLabel, slowCaseLabel);
    if (isAccessor) {
        patchBuffer.link(operationCall, operationFunction);
        patchBuffer.link(handlerCall, lookupExceptionHandler);
    }
    
    stubRoutine = FINALIZE_CODE_FOR_STUB(
        exec->codeBlock(), patchBuffer,
        ("Prototype chain access stub for %s, return point %p",
            toCString(*exec->codeBlock()).data(), successLabel.executableAddress()));
    return ProtoChainGenerationSucceeded;
}

static bool tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    CodeBlock* codeBlock = exec->codeBlock();
    VM* vm = &exec->vm();
    
    if (isJSArray(baseValue) && propertyName == exec->propertyNames().length) {
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
        GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
        GPRReg scratchGPR = TempRegisterSet(stubInfo.patch.usedRegisters).getFreeGPR();
        bool needToRestoreScratch = false;
        
        MacroAssembler stubJit;
        
        if (scratchGPR == InvalidGPRReg) {
#if USE(JSVALUE64)
            scratchGPR = AssemblyHelpers::selectScratchGPR(baseGPR, resultGPR);
#else
            scratchGPR = AssemblyHelpers::selectScratchGPR(baseGPR, resultGPR, resultTagGPR);
#endif
            stubJit.pushToSave(scratchGPR);
            needToRestoreScratch = true;
        }
        
        MacroAssembler::JumpList failureCases;
       
        stubJit.load8(MacroAssembler::Address(baseGPR, JSCell::indexingTypeOffset()), scratchGPR);
        failureCases.append(stubJit.branchTest32(MacroAssembler::Zero, scratchGPR, MacroAssembler::TrustedImm32(IsArray)));
        failureCases.append(stubJit.branchTest32(MacroAssembler::Zero, scratchGPR, MacroAssembler::TrustedImm32(IndexingShapeMask)));
        
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
        stubJit.load32(MacroAssembler::Address(scratchGPR, ArrayStorage::lengthOffset()), scratchGPR);
        failureCases.append(stubJit.branch32(MacroAssembler::LessThan, scratchGPR, MacroAssembler::TrustedImm32(0)));

        stubJit.move(scratchGPR, resultGPR);
#if USE(JSVALUE64)
        stubJit.or64(AssemblyHelpers::TrustedImm64(TagTypeNumber), resultGPR);
#elif USE(JSVALUE32_64)
        stubJit.move(AssemblyHelpers::TrustedImm32(0xffffffff), resultTagGPR); // JSValue::Int32Tag
#endif

        MacroAssembler::Jump success, fail;
        
        emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
        
        LinkBuffer patchBuffer(*vm, &stubJit, codeBlock);
        
        linkRestoreScratch(patchBuffer, needToRestoreScratch, stubInfo, success, fail, failureCases);
        
        stubInfo.stubRoutine = FINALIZE_CODE_FOR_STUB(
            exec->codeBlock(), patchBuffer,
            ("GetById array length stub for %s, return point %p",
                toCString(*exec->codeBlock()).data(), stubInfo.callReturnLocation.labelAtOffset(
                    stubInfo.patch.deltaCallToDone).executableAddress()));
        
        RepatchBuffer repatchBuffer(codeBlock);
        replaceWithJump(repatchBuffer, stubInfo, stubInfo.stubRoutine->code().code());
        repatchCall(repatchBuffer, stubInfo.callReturnLocation, operationGetById);
        
        return true;
    }
    
    // FIXME: should support length access for String.

    // FIXME: Cache property access for immediates.
    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    if (!slot.isCacheable())
        return false;
    if (!structure->propertyAccessesAreCacheable())
        return false;

    // Optimize self access.
    if (slot.slotBase() == baseValue) {
        if (!slot.isCacheableValue()
            || !MacroAssembler::isCompactPtrAlignedAddressOffset(maxOffsetRelativeToPatchedStorage(slot.cachedOffset()))) {
            repatchCall(codeBlock, stubInfo.callReturnLocation, operationGetByIdBuildList);
            return true;
        }

        repatchByIdSelfAccess(*vm, codeBlock, stubInfo, structure, propertyName, slot.cachedOffset(), operationGetByIdBuildList, true);
        stubInfo.initGetByIdSelf(*vm, codeBlock->ownerExecutable(), structure);
        return true;
    }
    
    if (structure->isDictionary())
        return false;

    if (stubInfo.patch.spillMode == NeedToSpill) {
        // We cannot do as much inline caching if the registers were not flushed prior to this GetById. In particular,
        // non-Value cached properties require planting calls, which requires registers to have been flushed. Thus,
        // if registers were not flushed, don't do non-Value caching.
        if (!slot.isCacheableValue())
            return false;
    }
    
    PropertyOffset offset = slot.cachedOffset();
    size_t count = normalizePrototypeChainForChainAccess(exec, baseValue, slot.slotBase(), propertyName, offset);
    if (count == InvalidPrototypeChain)
        return false;

    StructureChain* prototypeChain = structure->prototypeChain(exec);
    if (generateProtoChainAccessStub(exec, slot, propertyName, stubInfo, prototypeChain, count, offset,
        structure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone),
        stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase), stubInfo.stubRoutine) == ProtoChainGenerationFailed)
        return false;
    
    RepatchBuffer repatchBuffer(codeBlock);
    replaceWithJump(repatchBuffer, stubInfo, stubInfo.stubRoutine->code().code());
    repatchCall(repatchBuffer, stubInfo.callReturnLocation, operationGetByIdBuildList);
    
    stubInfo.initGetByIdChain(*vm, codeBlock->ownerExecutable(), structure, prototypeChain, count, slot.isCacheableValue());
    return true;
}

void repatchGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    bool cached = tryCacheGetByID(exec, baseValue, propertyName, slot, stubInfo);
    if (!cached)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
}

static bool getPolymorphicStructureList(
    VM* vm, CodeBlock* codeBlock, StructureStubInfo& stubInfo,
    PolymorphicAccessStructureList*& polymorphicStructureList, int& listIndex,
    CodeLocationLabel& slowCase)
{
    slowCase = stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase);
    
    if (stubInfo.accessType == access_unset) {
        RELEASE_ASSERT(!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList();
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 0, false);
        listIndex = 0;
    } else if (stubInfo.accessType == access_get_by_id_self) {
        RELEASE_ASSERT(!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList(*vm, codeBlock->ownerExecutable(), JITStubRoutine::createSelfManagedRoutine(slowCase), stubInfo.u.getByIdSelf.baseObjectStructure.get(), true);
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 1, true);
        listIndex = 1;
    } else if (stubInfo.accessType == access_get_by_id_chain) {
        RELEASE_ASSERT(!!stubInfo.stubRoutine);
        slowCase = CodeLocationLabel(stubInfo.stubRoutine->code().code());
        polymorphicStructureList = new PolymorphicAccessStructureList(*vm, codeBlock->ownerExecutable(), stubInfo.stubRoutine, stubInfo.u.getByIdChain.baseObjectStructure.get(), stubInfo.u.getByIdChain.chain.get(), stubInfo.u.getByIdChain.isDirect, stubInfo.u.getByIdChain.count);
        stubInfo.stubRoutine.clear();
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 1, false);
        listIndex = 1;
    } else {
        RELEASE_ASSERT(stubInfo.accessType == access_get_by_id_self_list);
        polymorphicStructureList = stubInfo.u.getByIdSelfList.structureList;
        listIndex = stubInfo.u.getByIdSelfList.listSize;
        slowCase = CodeLocationLabel(polymorphicStructureList->list[listIndex - 1].stubRoutine->code().code());
    }
    
    if (listIndex == POLYMORPHIC_LIST_CACHE_SIZE)
        return false;
    
    RELEASE_ASSERT(listIndex < POLYMORPHIC_LIST_CACHE_SIZE);
    return true;
}

static void patchJumpToGetByIdStub(CodeBlock* codeBlock, StructureStubInfo& stubInfo, JITStubRoutine* stubRoutine)
{
    RELEASE_ASSERT(stubInfo.accessType == access_get_by_id_self_list);
    RepatchBuffer repatchBuffer(codeBlock);
    if (stubInfo.u.getByIdSelfList.didSelfPatching) {
        repatchBuffer.relink(
            stubInfo.callReturnLocation.jumpAtOffset(
                stubInfo.patch.deltaCallToJump),
            CodeLocationLabel(stubRoutine->code().code()));
        return;
    }
    
    replaceWithJump(repatchBuffer, stubInfo, stubRoutine->code().code());
}

static bool tryBuildGetByIDList(ExecState* exec, JSValue baseValue, const Identifier& ident, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (!baseValue.isCell()
        || !slot.isCacheable()
        || !baseValue.asCell()->structure()->propertyAccessesAreCacheable())
        return false;

    CodeBlock* codeBlock = exec->codeBlock();
    VM* vm = &exec->vm();
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    
    if (slot.slotBase() == baseValue) {
        if (stubInfo.patch.spillMode == NeedToSpill) {
            // We cannot do as much inline caching if the registers were not flushed prior to this GetById. In particular,
            // non-Value cached properties require planting calls, which requires registers to have been flushed. Thus,
            // if registers were not flushed, don't do non-Value caching.
            if (!slot.isCacheableValue())
                return false;
        }
    
        PolymorphicAccessStructureList* polymorphicStructureList;
        int listIndex;
        CodeLocationLabel slowCase;

        if (!getPolymorphicStructureList(vm, codeBlock, stubInfo, polymorphicStructureList, listIndex, slowCase))
            return false;
        
        stubInfo.u.getByIdSelfList.listSize++;
        
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
        GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
        GPRReg scratchGPR = TempRegisterSet(stubInfo.patch.usedRegisters).getFreeGPR();
        
        CCallHelpers stubJit(vm, codeBlock);
        
        MacroAssembler::Jump wrongStruct = branchStructure(stubJit,
            MacroAssembler::NotEqual, 
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()), 
            structure);
        
        // The strategy we use for stubs is as follows:
        // 1) Call DFG helper that calls the getter.
        // 2) Check if there was an exception, and if there was, call yet another
        //    helper.
        
        bool isDirect = false;
        MacroAssembler::Call operationCall;
        MacroAssembler::Call handlerCall;
        FunctionPtr operationFunction;
        MacroAssembler::Jump success;
        
        if (slot.isCacheableGetter() || slot.isCacheableCustom()) {
            // FIXME: This code shouldn't be assuming that the top of stack is set up for JSC
            // JIT-style C calls, since we may be currently on top of an FTL frame.
            // https://bugs.webkit.org/show_bug.cgi?id=125711
            
            if (slot.isCacheableGetter()) {
                ASSERT(scratchGPR != InvalidGPRReg);
                ASSERT(baseGPR != scratchGPR);
                if (isInlineOffset(slot.cachedOffset())) {
#if USE(JSVALUE64)
                    stubJit.load64(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset())), scratchGPR);
#else
                    stubJit.load32(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset())), scratchGPR);
#endif
                } else {
                    stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR);
#if USE(JSVALUE64)
                    stubJit.load64(MacroAssembler::Address(scratchGPR, offsetRelativeToBase(slot.cachedOffset())), scratchGPR);
#else
                    stubJit.load32(MacroAssembler::Address(scratchGPR, offsetRelativeToBase(slot.cachedOffset())), scratchGPR);
#endif
                }
                stubJit.setupArgumentsWithExecState(baseGPR, scratchGPR);
                operationFunction = operationCallGetter;
            } else {
#if USE(JSVALUE64)
                // EncodedJSValue (*GetValueFunc)(ExecState*, JSObject* slotBase, EncodedJSValue thisValue, PropertyName);
                stubJit.setupArgumentsWithExecState(baseGPR, baseGPR, MacroAssembler::TrustedImmPtr(ident.impl()));
#else
                stubJit.setupArgumentsWithExecState(baseGPR, baseGPR, MacroAssembler::TrustedImm32(JSValue::CellTag), MacroAssembler::TrustedImmPtr(ident.impl()));
#endif
                operationFunction = FunctionPtr(slot.customGetter());
            }
            
            // Need to make sure that whenever this call is made in the future, we remember the
            // place that we made it from. It just so happens to be the place that we are at
            // right now!
            stubJit.store32(
                MacroAssembler::TrustedImm32(exec->locationAsRawBits()),
                CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
            stubJit.storePtr(GPRInfo::callFrameRegister, &vm->topCallFrame);
            
            operationCall = stubJit.call();
#if USE(JSVALUE64)
            stubJit.move(GPRInfo::returnValueGPR, resultGPR);
#else
            stubJit.setupResults(resultGPR, resultTagGPR);
#endif
            success = stubJit.emitExceptionCheck(CCallHelpers::InvertedExceptionCheck);
            
            stubJit.setupArguments(CCallHelpers::TrustedImmPtr(vm), GPRInfo::callFrameRegister);
            handlerCall = stubJit.call();
            stubJit.jumpToExceptionHandler();
        } else {
            if (isInlineOffset(slot.cachedOffset())) {
#if USE(JSVALUE64)
                stubJit.load64(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset())), resultGPR);
#else
                if (baseGPR == resultTagGPR) {
                    stubJit.load32(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
                    stubJit.load32(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
                } else {
                    stubJit.load32(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
                    stubJit.load32(MacroAssembler::Address(baseGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
                }
#endif
            } else {
                stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), resultGPR);
#if USE(JSVALUE64)
                stubJit.load64(MacroAssembler::Address(resultGPR, offsetRelativeToBase(slot.cachedOffset())), resultGPR);
#else
                stubJit.load32(MacroAssembler::Address(resultGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
                stubJit.load32(MacroAssembler::Address(resultGPR, offsetRelativeToBase(slot.cachedOffset()) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
#endif
            }
            success = stubJit.jump();
            isDirect = true;
        }

        LinkBuffer patchBuffer(*vm, &stubJit, codeBlock);
        
        patchBuffer.link(wrongStruct, slowCase);
        patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone));
        if (!isDirect) {
            patchBuffer.link(operationCall, operationFunction);
            patchBuffer.link(handlerCall, lookupExceptionHandler);
        }
        
        RefPtr<JITStubRoutine> stubRoutine =
            createJITStubRoutine(
                FINALIZE_CODE_FOR(
                    exec->codeBlock(), patchBuffer,
                    ("GetById polymorphic list access for %s, return point %p",
                        toCString(*exec->codeBlock()).data(), stubInfo.callReturnLocation.labelAtOffset(
                            stubInfo.patch.deltaCallToDone).executableAddress())),
                *vm,
                codeBlock->ownerExecutable(),
                slot.isCacheableGetter() || slot.isCacheableCustom());
        
        polymorphicStructureList->list[listIndex].set(*vm, codeBlock->ownerExecutable(), stubRoutine, structure, isDirect);
        
        patchJumpToGetByIdStub(codeBlock, stubInfo, stubRoutine.get());
        return listIndex < (POLYMORPHIC_LIST_CACHE_SIZE - 1);
    }
    
    if (baseValue.asCell()->structure()->typeInfo().prohibitsPropertyCaching()
        || baseValue.asCell()->structure()->isDictionary())
        return false;
    
    if (stubInfo.patch.spillMode == NeedToSpill) {
        // We cannot do as much inline caching if the registers were not flushed prior to this GetById. In particular,
        // non-Value cached properties require planting calls, which requires registers to have been flushed. Thus,
        // if registers were not flushed, don't do non-Value caching.
        if (!slot.isCacheableValue())
            return false;
    }
    

    PropertyOffset offset = slot.cachedOffset();
    size_t count = normalizePrototypeChainForChainAccess(exec, baseValue, slot.slotBase(), ident, offset);
    if (count == InvalidPrototypeChain)
        return false;

    StructureChain* prototypeChain = structure->prototypeChain(exec);
    
    PolymorphicAccessStructureList* polymorphicStructureList;
    int listIndex;
    CodeLocationLabel slowCase;
    if (!getPolymorphicStructureList(vm, codeBlock, stubInfo, polymorphicStructureList, listIndex, slowCase))
        return false;
    
    stubInfo.u.getByIdProtoList.listSize++;
    
    RefPtr<JITStubRoutine> stubRoutine;
    
    if (generateProtoChainAccessStub(exec, slot, ident, stubInfo, prototypeChain, count, offset, structure,
        stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone),
        slowCase, stubRoutine) == ProtoChainGenerationFailed)
        return false;
    
    polymorphicStructureList->list[listIndex].set(*vm, codeBlock->ownerExecutable(), stubRoutine, structure, prototypeChain, slot.isCacheableValue(), count);
    
    patchJumpToGetByIdStub(codeBlock, stubInfo, stubRoutine.get());
    
    return listIndex < (POLYMORPHIC_LIST_CACHE_SIZE - 1);
}

void buildGetByIDList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    bool dontChangeCall = tryBuildGetByIDList(exec, baseValue, propertyName, slot, stubInfo);
    if (!dontChangeCall)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
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

static V_JITOperation_ESsiJJI appropriateListBuildingPutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
{
    if (slot.isStrictMode()) {
        if (putKind == Direct)
            return operationPutByIdDirectStrictBuildList;
        return operationPutByIdStrictBuildList;
    }
    if (putKind == Direct)
        return operationPutByIdDirectNonStrictBuildList;
    return operationPutByIdNonStrictBuildList;
}

#if ENABLE(GGC)
static MacroAssembler::Call storeToWriteBarrierBuffer(CCallHelpers& jit, GPRReg cell, GPRReg scratch1, GPRReg scratch2, ScratchRegisterAllocator& allocator)
{
    ASSERT(scratch1 != scratch2);
    WriteBarrierBuffer* writeBarrierBuffer = &jit.vm()->heap.writeBarrierBuffer();
    jit.move(MacroAssembler::TrustedImmPtr(writeBarrierBuffer), scratch1);
    jit.load32(MacroAssembler::Address(scratch1, WriteBarrierBuffer::currentIndexOffset()), scratch2);
    MacroAssembler::Jump needToFlush = jit.branch32(MacroAssembler::AboveOrEqual, scratch2, MacroAssembler::Address(scratch1, WriteBarrierBuffer::capacityOffset()));

    jit.add32(MacroAssembler::TrustedImm32(1), scratch2);
    jit.store32(scratch2, MacroAssembler::Address(scratch1, WriteBarrierBuffer::currentIndexOffset()));

    jit.loadPtr(MacroAssembler::Address(scratch1, WriteBarrierBuffer::bufferOffset()), scratch1);
    // We use an offset of -sizeof(void*) because we already added 1 to scratch2.
    jit.storePtr(cell, MacroAssembler::BaseIndex(scratch1, scratch2, MacroAssembler::ScalePtr, static_cast<int32_t>(-sizeof(void*))));

    MacroAssembler::Jump done = jit.jump();
    needToFlush.link(&jit);

    ScratchBuffer* scratchBuffer = jit.vm()->scratchBufferForSize(allocator.desiredScratchBufferSize());
    allocator.preserveUsedRegistersToScratchBuffer(jit, scratchBuffer, scratch1);

    unsigned bytesFromBase = allocator.numberOfReusedRegisters() * sizeof(void*);
    unsigned bytesToSubtract = 0;
#if CPU(X86)
    bytesToSubtract += 2 * sizeof(void*);
    bytesFromBase += bytesToSubtract;
#endif
    unsigned currentAlignment = bytesFromBase % stackAlignmentBytes();
    bytesToSubtract += currentAlignment;

    if (bytesToSubtract)
        jit.subPtr(MacroAssembler::TrustedImm32(bytesToSubtract), MacroAssembler::stackPointerRegister); 

    jit.setupArgumentsWithExecState(cell);
    MacroAssembler::Call call = jit.call();

    if (bytesToSubtract)
        jit.addPtr(MacroAssembler::TrustedImm32(bytesToSubtract), MacroAssembler::stackPointerRegister);
    allocator.restoreUsedRegistersFromScratchBuffer(jit, scratchBuffer, scratch1);

    done.link(&jit);

    return call;
}

static MacroAssembler::Call writeBarrier(CCallHelpers& jit, GPRReg owner, GPRReg scratch1, GPRReg scratch2, ScratchRegisterAllocator& allocator)
{
    ASSERT(owner != scratch1);
    ASSERT(owner != scratch2);

#if ENABLE(DFG_JIT)
    MacroAssembler::Jump ownerNotMarkedOrAlreadyRemembered = DFG::SpeculativeJIT::checkMarkByte(jit, owner);
#endif
    MacroAssembler::Call call = storeToWriteBarrierBuffer(jit, owner, scratch1, scratch2, allocator);
#if ENABLE(DFG_JIT)
    ownerNotMarkedOrAlreadyRemembered.link(&jit);
#endif
    return call;
}
#endif // ENABLE(GGC)

static void emitPutReplaceStub(
    ExecState* exec,
    JSValue,
    const Identifier&,
    const PutPropertySlot& slot,
    StructureStubInfo& stubInfo,
    PutKind,
    Structure* structure,
    CodeLocationLabel failureLabel,
    RefPtr<JITStubRoutine>& stubRoutine)
{
    VM* vm = &exec->vm();
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg valueTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
    GPRReg valueGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);

    ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
    allocator.lock(baseGPR);
#if USE(JSVALUE32_64)
    allocator.lock(valueTagGPR);
#endif
    allocator.lock(valueGPR);
    
    GPRReg scratchGPR1 = allocator.allocateScratchGPR();
#if ENABLE(GGC)
    GPRReg scratchGPR2 = allocator.allocateScratchGPR();
#endif

    CCallHelpers stubJit(vm, exec->codeBlock());

    allocator.preserveReusedRegistersByPushing(stubJit);

    MacroAssembler::Jump badStructure = branchStructure(stubJit,
        MacroAssembler::NotEqual,
        MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
        structure);

#if USE(JSVALUE64)
    if (isInlineOffset(slot.cachedOffset()))
        stubJit.store64(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue)));
    else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR1);
        stubJit.store64(valueGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue)));
    }
#elif USE(JSVALUE32_64)
    if (isInlineOffset(slot.cachedOffset())) {
        stubJit.store32(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    } else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR1);
        stubJit.store32(valueGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
    
#if ENABLE(GGC)
    MacroAssembler::Call writeBarrierOperation = writeBarrier(stubJit, baseGPR, scratchGPR1, scratchGPR2, allocator);
#endif
    
    MacroAssembler::Jump success;
    MacroAssembler::Jump failure;
    
    if (allocator.didReuseRegisters()) {
        allocator.restoreReusedRegistersByPopping(stubJit);
        success = stubJit.jump();
        
        badStructure.link(&stubJit);
        allocator.restoreReusedRegistersByPopping(stubJit);
        failure = stubJit.jump();
    } else {
        success = stubJit.jump();
        failure = badStructure;
    }
    
    LinkBuffer patchBuffer(*vm, &stubJit, exec->codeBlock());
#if ENABLE(GGC)
    patchBuffer.link(writeBarrierOperation, operationFlushWriteBarrierBuffer);
#endif
    patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone));
    patchBuffer.link(failure, failureLabel);
            
    stubRoutine = FINALIZE_CODE_FOR_STUB(
        exec->codeBlock(), patchBuffer,
        ("PutById replace stub for %s, return point %p",
            toCString(*exec->codeBlock()).data(), stubInfo.callReturnLocation.labelAtOffset(
                stubInfo.patch.deltaCallToDone).executableAddress()));
}

static void emitPutTransitionStub(
    ExecState* exec,
    JSValue,
    const Identifier&,
    const PutPropertySlot& slot,
    StructureStubInfo& stubInfo,
    PutKind putKind,
    Structure* structure,
    Structure* oldStructure,
    StructureChain* prototypeChain,
    CodeLocationLabel failureLabel,
    RefPtr<JITStubRoutine>& stubRoutine)
{
    VM* vm = &exec->vm();

    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg valueTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
    GPRReg valueGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
    
    ScratchRegisterAllocator allocator(stubInfo.patch.usedRegisters);
    allocator.lock(baseGPR);
#if USE(JSVALUE32_64)
    allocator.lock(valueTagGPR);
#endif
    allocator.lock(valueGPR);
    
    CCallHelpers stubJit(vm);
    
    bool needThirdScratch = false;
    if (structure->outOfLineCapacity() != oldStructure->outOfLineCapacity()
        && oldStructure->outOfLineCapacity()) {
        needThirdScratch = true;
    }

    GPRReg scratchGPR1 = allocator.allocateScratchGPR();
    ASSERT(scratchGPR1 != baseGPR);
    ASSERT(scratchGPR1 != valueGPR);
    
    GPRReg scratchGPR2 = allocator.allocateScratchGPR();
    ASSERT(scratchGPR2 != baseGPR);
    ASSERT(scratchGPR2 != valueGPR);
    ASSERT(scratchGPR2 != scratchGPR1);

    GPRReg scratchGPR3;
    if (needThirdScratch) {
        scratchGPR3 = allocator.allocateScratchGPR();
        ASSERT(scratchGPR3 != baseGPR);
        ASSERT(scratchGPR3 != valueGPR);
        ASSERT(scratchGPR3 != scratchGPR1);
        ASSERT(scratchGPR3 != scratchGPR2);
    } else
        scratchGPR3 = InvalidGPRReg;
    
    allocator.preserveReusedRegistersByPushing(stubJit);

    MacroAssembler::JumpList failureCases;
            
    ASSERT(oldStructure->transitionWatchpointSetHasBeenInvalidated());
    
    failureCases.append(branchStructure(stubJit,
        MacroAssembler::NotEqual, 
        MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()), 
        oldStructure));
    
    addStructureTransitionCheck(
        oldStructure->storedPrototype(), exec->codeBlock(), stubInfo, stubJit, failureCases,
        scratchGPR1);
            
    if (putKind == NotDirect) {
        for (WriteBarrier<Structure>* it = prototypeChain->head(); *it; ++it) {
            addStructureTransitionCheck(
                (*it)->storedPrototype(), exec->codeBlock(), stubInfo, stubJit, failureCases,
                scratchGPR1);
        }
    }

    MacroAssembler::JumpList slowPath;
    
    bool scratchGPR1HasStorage = false;
    
    if (structure->outOfLineCapacity() != oldStructure->outOfLineCapacity()) {
        size_t newSize = structure->outOfLineCapacity() * sizeof(JSValue);
        CopiedAllocator* copiedAllocator = &vm->heap.storageAllocator();
        
        if (!oldStructure->outOfLineCapacity()) {
            stubJit.loadPtr(&copiedAllocator->m_currentRemaining, scratchGPR1);
            slowPath.append(stubJit.branchSubPtr(MacroAssembler::Signed, MacroAssembler::TrustedImm32(newSize), scratchGPR1));
            stubJit.storePtr(scratchGPR1, &copiedAllocator->m_currentRemaining);
            stubJit.negPtr(scratchGPR1);
            stubJit.addPtr(MacroAssembler::AbsoluteAddress(&copiedAllocator->m_currentPayloadEnd), scratchGPR1);
            stubJit.addPtr(MacroAssembler::TrustedImm32(sizeof(JSValue)), scratchGPR1);
        } else {
            size_t oldSize = oldStructure->outOfLineCapacity() * sizeof(JSValue);
            ASSERT(newSize > oldSize);
            
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR3);
            stubJit.loadPtr(&copiedAllocator->m_currentRemaining, scratchGPR1);
            slowPath.append(stubJit.branchSubPtr(MacroAssembler::Signed, MacroAssembler::TrustedImm32(newSize), scratchGPR1));
            stubJit.storePtr(scratchGPR1, &copiedAllocator->m_currentRemaining);
            stubJit.negPtr(scratchGPR1);
            stubJit.addPtr(MacroAssembler::AbsoluteAddress(&copiedAllocator->m_currentPayloadEnd), scratchGPR1);
            stubJit.addPtr(MacroAssembler::TrustedImm32(sizeof(JSValue)), scratchGPR1);
            // We have scratchGPR1 = new storage, scratchGPR3 = old storage, scratchGPR2 = available
            for (size_t offset = 0; offset < oldSize; offset += sizeof(void*)) {
                stubJit.loadPtr(MacroAssembler::Address(scratchGPR3, -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))), scratchGPR2);
                stubJit.storePtr(scratchGPR2, MacroAssembler::Address(scratchGPR1, -static_cast<ptrdiff_t>(offset + sizeof(JSValue) + sizeof(void*))));
            }
        }
        
        stubJit.storePtr(scratchGPR1, MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()));
        scratchGPR1HasStorage = true;
    }

    ASSERT(oldStructure->typeInfo().type() == structure->typeInfo().type());
    ASSERT(oldStructure->typeInfo().inlineTypeFlags() == structure->typeInfo().inlineTypeFlags());
    ASSERT(oldStructure->indexingType() == structure->indexingType());
    stubJit.store32(MacroAssembler::TrustedImm32(reinterpret_cast<uint32_t>(structure->id())), MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()));
#if USE(JSVALUE64)
    if (isInlineOffset(slot.cachedOffset()))
        stubJit.store64(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue)));
    else {
        if (!scratchGPR1HasStorage)
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR1);
        stubJit.store64(valueGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue)));
    }
#elif USE(JSVALUE32_64)
    if (isInlineOffset(slot.cachedOffset())) {
        stubJit.store32(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + offsetInInlineStorage(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    } else {
        if (!scratchGPR1HasStorage)
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::butterflyOffset()), scratchGPR1);
        stubJit.store32(valueGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(scratchGPR1, offsetInButterfly(slot.cachedOffset()) * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
    
#if ENABLE(GGC)
    MacroAssembler::Call writeBarrierOperation = writeBarrier(stubJit, baseGPR, scratchGPR1, scratchGPR2, allocator);
#endif
    
    MacroAssembler::Jump success;
    MacroAssembler::Jump failure;
            
    if (allocator.didReuseRegisters()) {
        allocator.restoreReusedRegistersByPopping(stubJit);
        success = stubJit.jump();

        failureCases.link(&stubJit);
        allocator.restoreReusedRegistersByPopping(stubJit);
        failure = stubJit.jump();
    } else
        success = stubJit.jump();
    
    MacroAssembler::Call operationCall;
    MacroAssembler::Jump successInSlowPath;
    
    if (structure->outOfLineCapacity() != oldStructure->outOfLineCapacity()) {
        slowPath.link(&stubJit);
        
        allocator.restoreReusedRegistersByPopping(stubJit);
        ScratchBuffer* scratchBuffer = vm->scratchBufferForSize(allocator.desiredScratchBufferSize());
        allocator.preserveUsedRegistersToScratchBuffer(stubJit, scratchBuffer, scratchGPR1);
#if USE(JSVALUE64)
        stubJit.setupArgumentsWithExecState(baseGPR, MacroAssembler::TrustedImmPtr(structure), MacroAssembler::TrustedImm32(slot.cachedOffset()), valueGPR);
#else
        stubJit.setupArgumentsWithExecState(baseGPR, MacroAssembler::TrustedImmPtr(structure), MacroAssembler::TrustedImm32(slot.cachedOffset()), valueGPR, valueTagGPR);
#endif
        operationCall = stubJit.call();
        allocator.restoreUsedRegistersFromScratchBuffer(stubJit, scratchBuffer, scratchGPR1);
        successInSlowPath = stubJit.jump();
    }
    
    LinkBuffer patchBuffer(*vm, &stubJit, exec->codeBlock());
#if ENABLE(GGC)
    patchBuffer.link(writeBarrierOperation, operationFlushWriteBarrierBuffer);
#endif
    patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone));
    if (allocator.didReuseRegisters())
        patchBuffer.link(failure, failureLabel);
    else
        patchBuffer.link(failureCases, failureLabel);
    if (structure->outOfLineCapacity() != oldStructure->outOfLineCapacity()) {
        patchBuffer.link(operationCall, operationReallocateStorageAndFinishPut);
        patchBuffer.link(successInSlowPath, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone));
    }
    
    stubRoutine =
        createJITStubRoutine(
            FINALIZE_CODE_FOR(
                exec->codeBlock(), patchBuffer,
                ("PutById %stransition stub (%p -> %p) for %s, return point %p",
                    structure->outOfLineCapacity() != oldStructure->outOfLineCapacity() ? "reallocating " : "",
                    oldStructure, structure,
                    toCString(*exec->codeBlock()).data(), stubInfo.callReturnLocation.labelAtOffset(
                        stubInfo.patch.deltaCallToDone).executableAddress())),
            *vm,
            exec->codeBlock()->ownerExecutable(),
            structure->outOfLineCapacity() != oldStructure->outOfLineCapacity(),
            structure);
}

static void emitCustomSetterStub(ExecState* exec, const PutPropertySlot& slot,
    StructureStubInfo& stubInfo, Structure* structure, StructureChain* prototypeChain,
    CodeLocationLabel failureLabel, RefPtr<JITStubRoutine>& stubRoutine)
{
    VM* vm = &exec->vm();
    ASSERT(stubInfo.patch.spillMode == DontSpill);
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg valueTagGPR = static_cast<GPRReg>(stubInfo.patch.valueTagGPR);
#endif
    GPRReg valueGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
    TempRegisterSet tempRegisters(stubInfo.patch.usedRegisters);

    CCallHelpers stubJit(vm);
    GPRReg scratchGPR = tempRegisters.getFreeGPR();
    RELEASE_ASSERT(scratchGPR != InvalidGPRReg);
    RELEASE_ASSERT(scratchGPR != baseGPR);
    RELEASE_ASSERT(scratchGPR != valueGPR);
    MacroAssembler::JumpList failureCases;
    failureCases.append(branchStructure(stubJit,
        MacroAssembler::NotEqual,
        MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
        structure));
    
    if (prototypeChain) {
        for (WriteBarrier<Structure>* it = prototypeChain->head(); *it; ++it)
            addStructureTransitionCheck((*it)->storedPrototype(), exec->codeBlock(), stubInfo, stubJit, failureCases, scratchGPR);
    }

    // typedef void (*PutValueFunc)(ExecState*, JSObject* base, EncodedJSValue thisObject, EncodedJSValue value);
#if USE(JSVALUE64)
    stubJit.setupArgumentsWithExecState(MacroAssembler::TrustedImmPtr(slot.base()), baseGPR, valueGPR);
#else
    stubJit.setupArgumentsWithExecState(MacroAssembler::TrustedImmPtr(slot.base()), baseGPR, MacroAssembler::TrustedImm32(JSValue::CellTag), valueGPR, valueTagGPR);
#endif

    // Need to make sure that whenever this call is made in the future, we remember the
    // place that we made it from. It just so happens to be the place that we are at
    // right now!
    stubJit.store32(MacroAssembler::TrustedImm32(exec->locationAsRawBits()),
        CCallHelpers::tagFor(static_cast<VirtualRegister>(JSStack::ArgumentCount)));
    stubJit.storePtr(GPRInfo::callFrameRegister, &vm->topCallFrame);

    MacroAssembler::Call setterCall = stubJit.call();
    
    MacroAssembler::Jump success = stubJit.emitExceptionCheck(CCallHelpers::InvertedExceptionCheck);

    stubJit.setupArguments(CCallHelpers::TrustedImmPtr(vm), GPRInfo::callFrameRegister);

    MacroAssembler::Call handlerCall = stubJit.call();

    stubJit.jumpToExceptionHandler();
    LinkBuffer patchBuffer(*vm, &stubJit, exec->codeBlock());

    patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone));
    patchBuffer.link(failureCases, failureLabel);
    patchBuffer.link(setterCall, FunctionPtr(slot.customSetter()));
    patchBuffer.link(handlerCall, lookupExceptionHandler);

    stubRoutine = createJITStubRoutine(
        FINALIZE_CODE_FOR(exec->codeBlock(), patchBuffer, ("PutById custom setter stub for %s, return point %p",
        toCString(*exec->codeBlock()).data(), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone).executableAddress())), *vm, exec->codeBlock()->ownerExecutable(), structure);
}


static bool tryCachePutByID(ExecState* exec, JSValue baseValue, const Identifier& ident, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    CodeBlock* codeBlock = exec->codeBlock();
    VM* vm = &exec->vm();

    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    Structure* oldStructure = structure->previousID();
    
    if (!slot.isCacheablePut() && !slot.isCacheableCustomProperty())
        return false;
    if (!structure->propertyAccessesAreCacheable())
        return false;

    // Optimize self access.
    if (slot.base() == baseValue && slot.isCacheablePut()) {
        if (slot.type() == PutPropertySlot::NewProperty) {
            if (structure->isDictionary())
                return false;
            
            // Skip optimizing the case where we need a realloc, if we don't have
            // enough registers to make it happen.
            if (GPRInfo::numberOfRegisters < 6
                && oldStructure->outOfLineCapacity() != structure->outOfLineCapacity()
                && oldStructure->outOfLineCapacity())
                return false;
            
            // Skip optimizing the case where we need realloc, and the structure has
            // indexing storage.
            if (oldStructure->couldHaveIndexingHeader())
                return false;
            
            if (normalizePrototypeChain(exec, baseCell) == InvalidPrototypeChain)
                return false;
            
            StructureChain* prototypeChain = structure->prototypeChain(exec);
            
            emitPutTransitionStub(
                exec, baseValue, ident, slot, stubInfo, putKind,
                structure, oldStructure, prototypeChain,
                stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase),
                stubInfo.stubRoutine);
            
            RepatchBuffer repatchBuffer(codeBlock);
            repatchBuffer.relink(
                stubInfo.callReturnLocation.jumpAtOffset(
                    stubInfo.patch.deltaCallToJump),
                CodeLocationLabel(stubInfo.stubRoutine->code().code()));
            repatchCall(repatchBuffer, stubInfo.callReturnLocation, appropriateListBuildingPutByIdFunction(slot, putKind));
            
            stubInfo.initPutByIdTransition(*vm, codeBlock->ownerExecutable(), oldStructure, structure, prototypeChain, putKind == Direct);
            
            return true;
        }

        if (!MacroAssembler::isPtrAlignedAddressOffset(offsetRelativeToPatchedStorage(slot.cachedOffset())))
            return false;

        repatchByIdSelfAccess(*vm, codeBlock, stubInfo, structure, ident, slot.cachedOffset(), appropriateListBuildingPutByIdFunction(slot, putKind), false);
        stubInfo.initPutByIdReplace(*vm, codeBlock->ownerExecutable(), structure);
        return true;
    }
    if (slot.isCacheableCustomProperty() && stubInfo.patch.spillMode == DontSpill) {
        RefPtr<JITStubRoutine> stubRoutine;

        StructureChain* prototypeChain = 0;
        if (baseValue != slot.base()) {
            PropertyOffset offsetIgnored;
            if (normalizePrototypeChainForChainAccess(exec, baseCell, slot.base(), ident, offsetIgnored) == InvalidPrototypeChain)
                return false;

            prototypeChain = structure->prototypeChain(exec);
        }
        PolymorphicPutByIdList* list;
        list = PolymorphicPutByIdList::from(putKind, stubInfo);

        emitCustomSetterStub(exec, slot, stubInfo,
            structure, prototypeChain,
            stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase),
            stubRoutine);

        list->addAccess(PutByIdAccess::customSetter(*vm, codeBlock->ownerExecutable(), structure, prototypeChain, slot.customSetter(), stubRoutine));

        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), CodeLocationLabel(stubRoutine->code().code()));
        repatchCall(repatchBuffer, stubInfo.callReturnLocation, appropriateListBuildingPutByIdFunction(slot, putKind));
        RELEASE_ASSERT(!list->isFull());
        return true;
    }

    return false;
}

void repatchPutByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    bool cached = tryCachePutByID(exec, baseValue, propertyName, slot, stubInfo, putKind);
    if (!cached)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

static bool tryBuildPutByIdList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    CodeBlock* codeBlock = exec->codeBlock();
    VM* vm = &exec->vm();

    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    Structure* oldStructure = structure->previousID();
    
    
    if (!slot.isCacheablePut() && !slot.isCacheableCustomProperty())
        return false;

    if (!structure->propertyAccessesAreCacheable())
        return false;

    // Optimize self access.
    if (slot.base() == baseValue && slot.isCacheablePut()) {
        PolymorphicPutByIdList* list;
        RefPtr<JITStubRoutine> stubRoutine;
        
        if (slot.type() == PutPropertySlot::NewProperty) {
            if (structure->isDictionary())
                return false;
            
            // Skip optimizing the case where we need a realloc, if we don't have
            // enough registers to make it happen.
            if (GPRInfo::numberOfRegisters < 6
                && oldStructure->outOfLineCapacity() != structure->outOfLineCapacity()
                && oldStructure->outOfLineCapacity())
                return false;
            
            // Skip optimizing the case where we need realloc, and the structure has
            // indexing storage.
            if (oldStructure->couldHaveIndexingHeader())
                return false;
            
            if (normalizePrototypeChain(exec, baseCell) == InvalidPrototypeChain)
                return false;
            
            StructureChain* prototypeChain = structure->prototypeChain(exec);
            
            // We're now committed to creating the stub. Mogrify the meta-data accordingly.
            list = PolymorphicPutByIdList::from(putKind, stubInfo);
            
            emitPutTransitionStub(
                exec, baseValue, propertyName, slot, stubInfo, putKind,
                structure, oldStructure, prototypeChain,
                CodeLocationLabel(list->currentSlowPathTarget()),
                stubRoutine);
            
            list->addAccess(
                PutByIdAccess::transition(
                    *vm, codeBlock->ownerExecutable(),
                    oldStructure, structure, prototypeChain,
                    stubRoutine));
        } else {
            // We're now committed to creating the stub. Mogrify the meta-data accordingly.
            list = PolymorphicPutByIdList::from(putKind, stubInfo);
            
            emitPutReplaceStub(
                exec, baseValue, propertyName, slot, stubInfo, putKind,
                structure, CodeLocationLabel(list->currentSlowPathTarget()), stubRoutine);
            
            list->addAccess(
                PutByIdAccess::replace(
                    *vm, codeBlock->ownerExecutable(),
                    structure, stubRoutine));
        }
        
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), CodeLocationLabel(stubRoutine->code().code()));
        
        if (list->isFull())
            repatchCall(repatchBuffer, stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
        
        return true;
    }

    if (slot.isCacheableCustomProperty() && stubInfo.patch.spillMode == DontSpill) {
        RefPtr<JITStubRoutine> stubRoutine;
        StructureChain* prototypeChain = 0;
        if (baseValue != slot.base()) {
            PropertyOffset offsetIgnored;
            if (normalizePrototypeChainForChainAccess(exec, baseCell, slot.base(), propertyName, offsetIgnored) == InvalidPrototypeChain)
                return false;

            prototypeChain = structure->prototypeChain(exec);
        }
        PolymorphicPutByIdList* list;
        list = PolymorphicPutByIdList::from(putKind, stubInfo);

        emitCustomSetterStub(exec, slot, stubInfo,
            structure, prototypeChain,
            CodeLocationLabel(list->currentSlowPathTarget()),
            stubRoutine);

        list->addAccess(PutByIdAccess::customSetter(*vm, codeBlock->ownerExecutable(), structure, prototypeChain, slot.customSetter(), stubRoutine));

        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), CodeLocationLabel(stubRoutine->code().code()));
        if (list->isFull())
            repatchCall(repatchBuffer, stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));

        return true;
    }
    return false;
}

void buildPutByIdList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    GCSafeConcurrentJITLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);
    
    bool cached = tryBuildPutByIdList(exec, baseValue, propertyName, slot, stubInfo, putKind);
    if (!cached)
        repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

static bool tryRepatchIn(
    ExecState* exec, JSCell* base, const Identifier& ident, bool wasFound,
    const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (!base->structure()->propertyAccessesAreCacheable())
        return false;
    
    if (wasFound) {
        if (!slot.isCacheable())
            return false;
    }
    
    CodeBlock* codeBlock = exec->codeBlock();
    VM* vm = &exec->vm();
    Structure* structure = base->structure();
    
    PropertyOffset offsetIgnored;
    size_t count = normalizePrototypeChainForChainAccess(exec, base, wasFound ? slot.slotBase() : JSValue(), ident, offsetIgnored);
    if (count == InvalidPrototypeChain)
        return false;
    
    PolymorphicAccessStructureList* polymorphicStructureList;
    int listIndex;
    
    CodeLocationLabel successLabel = stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToDone);
    CodeLocationLabel slowCaseLabel;
    
    if (stubInfo.accessType == access_unset) {
        polymorphicStructureList = new PolymorphicAccessStructureList();
        stubInfo.initInList(polymorphicStructureList, 0);
        slowCaseLabel = stubInfo.callReturnLocation.labelAtOffset(
            stubInfo.patch.deltaCallToSlowCase);
        listIndex = 0;
    } else {
        RELEASE_ASSERT(stubInfo.accessType == access_in_list);
        polymorphicStructureList = stubInfo.u.inList.structureList;
        listIndex = stubInfo.u.inList.listSize;
        slowCaseLabel = CodeLocationLabel(polymorphicStructureList->list[listIndex - 1].stubRoutine->code().code());
        
        if (listIndex == POLYMORPHIC_LIST_CACHE_SIZE)
            return false;
    }
    
    StructureChain* chain = structure->prototypeChain(exec);
    RefPtr<JITStubRoutine> stubRoutine;
    
    {
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.baseGPR);
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.valueGPR);
        GPRReg scratchGPR = TempRegisterSet(stubInfo.patch.usedRegisters).getFreeGPR();
        
        CCallHelpers stubJit(vm);
        
        bool needToRestoreScratch;
        if (scratchGPR == InvalidGPRReg) {
            scratchGPR = AssemblyHelpers::selectScratchGPR(baseGPR, resultGPR);
            stubJit.pushToSave(scratchGPR);
            needToRestoreScratch = true;
        } else
            needToRestoreScratch = false;
        
        MacroAssembler::JumpList failureCases;
        failureCases.append(branchStructure(stubJit,
            MacroAssembler::NotEqual,
            MacroAssembler::Address(baseGPR, JSCell::structureIDOffset()),
            structure));

        CodeBlock* codeBlock = exec->codeBlock();
        if (structure->typeInfo().newImpurePropertyFiresWatchpoints())
            vm->registerWatchpointForImpureProperty(ident, stubInfo.addWatchpoint(codeBlock));

        Structure* currStructure = structure;
        WriteBarrier<Structure>* it = chain->head();
        for (unsigned i = 0; i < count; ++i, ++it) {
            JSObject* prototype = asObject(currStructure->prototypeForLookup(exec));
            Structure* protoStructure = prototype->structure();
            addStructureTransitionCheck(
                prototype, protoStructure, exec->codeBlock(), stubInfo, stubJit,
                failureCases, scratchGPR);
            if (protoStructure->typeInfo().newImpurePropertyFiresWatchpoints())
                vm->registerWatchpointForImpureProperty(ident, stubInfo.addWatchpoint(codeBlock));
            currStructure = it->get();
        }
        
#if USE(JSVALUE64)
        stubJit.move(MacroAssembler::TrustedImm64(JSValue::encode(jsBoolean(wasFound))), resultGPR);
#else
        stubJit.move(MacroAssembler::TrustedImm32(wasFound), resultGPR);
#endif
        
        MacroAssembler::Jump success, fail;
        
        emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
        
        LinkBuffer patchBuffer(*vm, &stubJit, exec->codeBlock());

        linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, successLabel, slowCaseLabel);
        
        stubRoutine = FINALIZE_CODE_FOR_STUB(
            exec->codeBlock(), patchBuffer,
            ("In (found = %s) stub for %s, return point %p",
                wasFound ? "yes" : "no", toCString(*exec->codeBlock()).data(),
                successLabel.executableAddress()));
    }
    
    polymorphicStructureList->list[listIndex].set(*vm, codeBlock->ownerExecutable(), stubRoutine, structure, true);
    stubInfo.u.inList.listSize++;
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), CodeLocationLabel(stubRoutine->code().code()));
    
    return listIndex < (POLYMORPHIC_LIST_CACHE_SIZE - 1);
}

void repatchIn(
    ExecState* exec, JSCell* base, const Identifier& ident, bool wasFound,
    const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (tryRepatchIn(exec, base, ident, wasFound, slot, stubInfo))
        return;
    repatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationIn);
}

static void linkSlowFor(
    RepatchBuffer& repatchBuffer, VM* vm, CallLinkInfo& callLinkInfo,
    CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    repatchBuffer.relink(
        callLinkInfo.callReturnLocation,
        vm->getCTIStub(virtualThunkGeneratorFor(kind, registers)).code());
}

void linkFor(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    JSFunction* callee, MacroAssemblerCodePtr codePtr, CodeSpecializationKind kind,
    RegisterPreservationMode registers)
{
    ASSERT(!callLinkInfo.stub);
    
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();

    // If you're being call-linked from a DFG caller then you obviously didn't get inlined.
    if (calleeCodeBlock && JITCode::isOptimizingJIT(callerCodeBlock->jitType()))
        calleeCodeBlock->m_shouldAlwaysBeInlined = false;
    
    VM* vm = callerCodeBlock->vm();
    
    RepatchBuffer repatchBuffer(callerCodeBlock);
    
    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.callee.set(exec->callerFrame()->vm(), callLinkInfo.hotPathBegin, callerCodeBlock->ownerExecutable(), callee);
    callLinkInfo.lastSeenCallee.set(exec->callerFrame()->vm(), callerCodeBlock->ownerExecutable(), callee);
    if (shouldShowDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", *callerCodeBlock, " at ", callLinkInfo.codeOrigin, " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");
    repatchBuffer.relink(callLinkInfo.hotPathOther, codePtr);
    
    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(exec->callerFrame(), &callLinkInfo);
    
    if (kind == CodeForCall) {
        repatchBuffer.relink(callLinkInfo.callReturnLocation, vm->getCTIStub(linkClosureCallThunkGeneratorFor(registers)).code());
        return;
    }
    
    ASSERT(kind == CodeForConstruct);
    linkSlowFor(repatchBuffer, vm, callLinkInfo, CodeForConstruct, registers);
}

void linkSlowFor(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeSpecializationKind kind,
    RegisterPreservationMode registers)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();
    
    RepatchBuffer repatchBuffer(callerCodeBlock);
    
    linkSlowFor(repatchBuffer, vm, callLinkInfo, kind, registers);
}

void linkClosureCall(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    Structure* structure, ExecutableBase* executable, MacroAssemblerCodePtr codePtr,
    RegisterPreservationMode registers)
{
    ASSERT(!callLinkInfo.stub);
    
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();
    
    GPRReg calleeGPR = static_cast<GPRReg>(callLinkInfo.calleeGPR);
    
    CCallHelpers stubJit(vm, callerCodeBlock);
    
    CCallHelpers::JumpList slowPath;
    
    ptrdiff_t offsetToFrame = -sizeof(CallerFrameAndPC);

    if (!ASSERT_DISABLED) {
        CCallHelpers::Jump okArgumentCount = stubJit.branch32(
            CCallHelpers::Below, CCallHelpers::Address(CCallHelpers::stackPointerRegister, static_cast<ptrdiff_t>(sizeof(Register) * JSStack::ArgumentCount) + offsetToFrame + PayloadOffset), CCallHelpers::TrustedImm32(10000000));
        stubJit.breakpoint();
        okArgumentCount.link(&stubJit);
    }

#if USE(JSVALUE64)
    // We can safely clobber everything except the calleeGPR. We can't rely on tagMaskRegister
    // being set. So we do this the hard way.
    GPRReg scratch = AssemblyHelpers::selectScratchGPR(calleeGPR);
    stubJit.move(MacroAssembler::TrustedImm64(TagMask), scratch);
    slowPath.append(stubJit.branchTest64(CCallHelpers::NonZero, calleeGPR, scratch));
#else
    // We would have already checked that the callee is a cell.
#endif
    
    slowPath.append(
        branchStructure(stubJit,
            CCallHelpers::NotEqual,
            CCallHelpers::Address(calleeGPR, JSCell::structureIDOffset()),
            structure));
    
    slowPath.append(
        stubJit.branchPtr(
            CCallHelpers::NotEqual,
            CCallHelpers::Address(calleeGPR, JSFunction::offsetOfExecutable()),
            CCallHelpers::TrustedImmPtr(executable)));
    
    stubJit.loadPtr(
        CCallHelpers::Address(calleeGPR, JSFunction::offsetOfScopeChain()),
        GPRInfo::returnValueGPR);
    
#if USE(JSVALUE64)
    stubJit.store64(
        GPRInfo::returnValueGPR,
        CCallHelpers::Address(MacroAssembler::stackPointerRegister, static_cast<ptrdiff_t>(sizeof(Register) * JSStack::ScopeChain) + offsetToFrame));
#else
    stubJit.storePtr(
        GPRInfo::returnValueGPR,
        CCallHelpers::Address(MacroAssembler::stackPointerRegister, static_cast<ptrdiff_t>(sizeof(Register) * JSStack::ScopeChain) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload) + offsetToFrame));
    stubJit.store32(
        CCallHelpers::TrustedImm32(JSValue::CellTag),
        CCallHelpers::Address(MacroAssembler::stackPointerRegister, static_cast<ptrdiff_t>(sizeof(Register) * JSStack::ScopeChain) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag) + offsetToFrame));
#endif
    
    AssemblyHelpers::Call call = stubJit.nearCall();
    AssemblyHelpers::Jump done = stubJit.jump();
    
    slowPath.link(&stubJit);
    stubJit.move(calleeGPR, GPRInfo::regT0);
#if USE(JSVALUE32_64)
    stubJit.move(CCallHelpers::TrustedImm32(JSValue::CellTag), GPRInfo::regT1);
#endif
    stubJit.move(CCallHelpers::TrustedImmPtr(callLinkInfo.callReturnLocation.executableAddress()), GPRInfo::regT2);
    
    stubJit.restoreReturnAddressBeforeReturn(GPRInfo::regT2);
    AssemblyHelpers::Jump slow = stubJit.jump();
    
    LinkBuffer patchBuffer(*vm, &stubJit, callerCodeBlock);
    
    patchBuffer.link(call, FunctionPtr(codePtr.executableAddress()));
    if (JITCode::isOptimizingJIT(callerCodeBlock->jitType()))
        patchBuffer.link(done, callLinkInfo.callReturnLocation.labelAtOffset(0));
    else
        patchBuffer.link(done, callLinkInfo.hotPathOther.labelAtOffset(0));
    patchBuffer.link(slow, CodeLocationLabel(vm->getCTIStub(virtualThunkGeneratorFor(CodeForCall, registers)).code()));
    
    RefPtr<ClosureCallStubRoutine> stubRoutine = adoptRef(new ClosureCallStubRoutine(
        FINALIZE_CODE_FOR(
            callerCodeBlock, patchBuffer,
            ("Closure call stub for %s, return point %p, target %p (%s)",
                toCString(*callerCodeBlock).data(), callLinkInfo.callReturnLocation.labelAtOffset(0).executableAddress(),
                codePtr.executableAddress(), toCString(pointerDump(calleeCodeBlock)).data())),
        *vm, callerCodeBlock->ownerExecutable(), structure, executable, callLinkInfo.codeOrigin));
    
    RepatchBuffer repatchBuffer(callerCodeBlock);
    
    repatchBuffer.replaceWithJump(
        RepatchBuffer::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin),
        CodeLocationLabel(stubRoutine->code().code()));
    linkSlowFor(repatchBuffer, vm, callLinkInfo, CodeForCall, registers);
    
    callLinkInfo.stub = stubRoutine.release();
    
    ASSERT(!calleeCodeBlock || calleeCodeBlock->isIncomingCallAlreadyLinked(&callLinkInfo));
}

void resetGetByID(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    repatchCall(repatchBuffer, stubInfo.callReturnLocation, operationGetByIdOptimize);
    CodeLocationDataLabel32 structureLabel = stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall);
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        repatchBuffer.revertJumpReplacementToPatchableBranch32WithPatch(
            RepatchBuffer::startOfPatchableBranch32WithPatchOnAddress(structureLabel),
            MacroAssembler::Address(
                static_cast<MacroAssembler::RegisterID>(stubInfo.patch.baseGPR),
                JSCell::structureIDOffset()),
            static_cast<int32_t>(unusedPointer));
    }
    repatchBuffer.repatch(structureLabel, static_cast<int32_t>(unusedPointer));
#if USE(JSVALUE64)
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToLoadOrStore), 0);
#else
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), 0);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), 0);
#endif
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

void resetPutByID(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    V_JITOperation_ESsiJJI unoptimizedFunction = bitwise_cast<V_JITOperation_ESsiJJI>(readCallTarget(repatchBuffer, stubInfo.callReturnLocation).executableAddress());
    V_JITOperation_ESsiJJI optimizedFunction;
    if (unoptimizedFunction == operationPutByIdStrict || unoptimizedFunction == operationPutByIdStrictBuildList)
        optimizedFunction = operationPutByIdStrictOptimize;
    else if (unoptimizedFunction == operationPutByIdNonStrict || unoptimizedFunction == operationPutByIdNonStrictBuildList)
        optimizedFunction = operationPutByIdNonStrictOptimize;
    else if (unoptimizedFunction == operationPutByIdDirectStrict || unoptimizedFunction == operationPutByIdDirectStrictBuildList)
        optimizedFunction = operationPutByIdDirectStrictOptimize;
    else {
        ASSERT(unoptimizedFunction == operationPutByIdDirectNonStrict || unoptimizedFunction == operationPutByIdDirectNonStrictBuildList);
        optimizedFunction = operationPutByIdDirectNonStrictOptimize;
    }
    repatchCall(repatchBuffer, stubInfo.callReturnLocation, optimizedFunction);
    CodeLocationDataLabel32 structureLabel = stubInfo.callReturnLocation.dataLabel32AtOffset(-(intptr_t)stubInfo.patch.deltaCheckImmToCall);
    if (MacroAssembler::canJumpReplacePatchableBranch32WithPatch()) {
        repatchBuffer.revertJumpReplacementToPatchableBranch32WithPatch(
            RepatchBuffer::startOfPatchableBranch32WithPatchOnAddress(structureLabel),
            MacroAssembler::Address(
                static_cast<MacroAssembler::RegisterID>(stubInfo.patch.baseGPR),
                JSCell::structureIDOffset()),
            static_cast<int32_t>(unusedPointer));
    }
    repatchBuffer.repatch(structureLabel, static_cast<int32_t>(unusedPointer));
#if USE(JSVALUE64)
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToLoadOrStore), 0);
#else
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToTagLoadOrStore), 0);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.deltaCallToPayloadLoadOrStore), 0);
#endif
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

void resetIn(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.deltaCallToJump), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.deltaCallToSlowCase));
}

} // namespace JSC

#endif
