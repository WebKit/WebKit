/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGRepatch.h"

#if ENABLE(DFG_JIT)

#include "DFGCCallHelpers.h"
#include "DFGSpeculativeJIT.h"
#include "LinkBuffer.h"
#include "Operations.h"
#include "PolymorphicPutByIdList.h"
#include "RepatchBuffer.h"

namespace JSC { namespace DFG {

static void dfgRepatchCall(CodeBlock* codeblock, CodeLocationCall call, FunctionPtr newCalleeFunction)
{
    RepatchBuffer repatchBuffer(codeblock);
    repatchBuffer.relink(call, newCalleeFunction);
}

static void dfgRepatchByIdSelfAccess(CodeBlock* codeBlock, StructureStubInfo& stubInfo, Structure* structure, size_t offset, const FunctionPtr &slowPathFunction, bool compact)
{
    RepatchBuffer repatchBuffer(codeBlock);

    // Only optimize once!
    repatchBuffer.relink(stubInfo.callReturnLocation, slowPathFunction);

    // Patch the structure check & the offset of the load.
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelPtrAtOffset(-(intptr_t)stubInfo.patch.dfg.deltaCheckImmToCall), structure);
#if USE(JSVALUE64)
    if (compact)
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToLoadOrStore), sizeof(JSValue) * offset);
    else
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToLoadOrStore), sizeof(JSValue) * offset);
#elif USE(JSVALUE32_64)
    if (compact) {
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToTagLoadOrStore), sizeof(JSValue) * offset + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToPayloadLoadOrStore), sizeof(JSValue) * offset + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    } else {
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToTagLoadOrStore), sizeof(JSValue) * offset + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag));
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToPayloadLoadOrStore), sizeof(JSValue) * offset + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload));
    }
#endif
}

static void emitRestoreScratch(MacroAssembler& stubJit, bool needToRestoreScratch, GPRReg scratchGPR, MacroAssembler::Jump& success, MacroAssembler::Jump& fail, MacroAssembler::JumpList failureCases)
{
    if (needToRestoreScratch) {
        stubJit.pop(scratchGPR);
        
        success = stubJit.jump();
        
        // link failure cases here, so we can pop scratchGPR, and then jump back.
        failureCases.link(&stubJit);
        
        stubJit.pop(scratchGPR);
        
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
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase));
}

static void generateProtoChainAccessStub(ExecState* exec, StructureStubInfo& stubInfo, StructureChain* chain, size_t count, size_t offset, Structure* structure, CodeLocationLabel successLabel, CodeLocationLabel slowCaseLabel, MacroAssemblerCodeRef& stubRoutine)
{
    JSGlobalData* globalData = &exec->globalData();

    MacroAssembler stubJit;
        
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.dfg.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueTagGPR);
#endif
    GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueGPR);
    GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.patch.dfg.scratchGPR);
    bool needToRestoreScratch = false;
    
    if (scratchGPR == InvalidGPRReg) {
        scratchGPR = SpeculativeJIT::selectScratchGPR(baseGPR, resultGPR);
        stubJit.push(scratchGPR);
        needToRestoreScratch = true;
    }
    
    MacroAssembler::JumpList failureCases;
    
    failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(structure)));
    
    Structure* currStructure = structure;
    WriteBarrier<Structure>* it = chain->head();
    JSObject* protoObject = 0;
    for (unsigned i = 0; i < count; ++i, ++it) {
        protoObject = asObject(currStructure->prototypeForLookup(exec));
        stubJit.move(MacroAssembler::TrustedImmPtr(protoObject), scratchGPR);
        failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(scratchGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(protoObject->structure())));
        currStructure = it->get();
    }
    
    stubJit.loadPtr(protoObject->addressOfPropertyStorage(), resultGPR);
#if USE(JSVALUE64)
    stubJit.loadPtr(MacroAssembler::Address(resultGPR, offset * sizeof(WriteBarrier<Unknown>)), resultGPR);
#elif USE(JSVALUE32_64)
    stubJit.load32(MacroAssembler::Address(resultGPR, offset * sizeof(WriteBarrier<Unknown>) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
    stubJit.load32(MacroAssembler::Address(resultGPR, offset * sizeof(WriteBarrier<Unknown>) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
#endif

    MacroAssembler::Jump success, fail;
    
    emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
    
    LinkBuffer patchBuffer(*globalData, &stubJit, exec->codeBlock());
    
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, successLabel, slowCaseLabel);
    
    stubRoutine = patchBuffer.finalizeCode();
}

static bool tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();
    
    if (isJSArray(baseValue) && propertyName == exec->propertyNames().length) {
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.dfg.baseGPR);
#if USE(JSVALUE32_64)
        GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueTagGPR);
#endif
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueGPR);
        GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.patch.dfg.scratchGPR);
        bool needToRestoreScratch = false;
        
        MacroAssembler stubJit;
        
        if (scratchGPR == InvalidGPRReg) {
            scratchGPR = SpeculativeJIT::selectScratchGPR(baseGPR, resultGPR);
            stubJit.push(scratchGPR);
            needToRestoreScratch = true;
        }
        
        MacroAssembler::JumpList failureCases;
        
        failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::classInfoOffset()), MacroAssembler::TrustedImmPtr(&JSArray::s_info)));
        
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), scratchGPR);
        stubJit.load32(MacroAssembler::Address(scratchGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), scratchGPR);
        failureCases.append(stubJit.branch32(MacroAssembler::LessThan, scratchGPR, MacroAssembler::TrustedImm32(0)));

#if USE(JSVALUE64)
        stubJit.orPtr(GPRInfo::tagTypeNumberRegister, scratchGPR, resultGPR);
#elif USE(JSVALUE32_64)
        stubJit.move(scratchGPR, resultGPR);
        stubJit.move(JITCompiler::TrustedImm32(0xffffffff), resultTagGPR); // JSValue::Int32Tag
#endif

        MacroAssembler::Jump success, fail;
        
        emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
        
        LinkBuffer patchBuffer(*globalData, &stubJit, codeBlock);
        
        linkRestoreScratch(patchBuffer, needToRestoreScratch, stubInfo, success, fail, failureCases);
        
        stubInfo.stubRoutine = patchBuffer.finalizeCode();
        
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), CodeLocationLabel(stubInfo.stubRoutine.code()));
        repatchBuffer.relink(stubInfo.callReturnLocation, operationGetById);
        
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
    if (structure->isUncacheableDictionary() || structure->typeInfo().prohibitsPropertyCaching())
        return false;

    // Optimize self access.
    if (slot.slotBase() == baseValue) {
        if ((slot.cachedPropertyType() != PropertySlot::Value) || ((slot.cachedOffset() * sizeof(JSValue)) > (unsigned)MacroAssembler::MaximumCompactPtrAlignedAddressOffset)) {
            dfgRepatchCall(codeBlock, stubInfo.callReturnLocation, operationGetByIdBuildList);
            return true;
        }

        dfgRepatchByIdSelfAccess(codeBlock, stubInfo, structure, slot.cachedOffset(), operationGetByIdBuildList, true);
        stubInfo.initGetByIdSelf(*globalData, codeBlock->ownerExecutable(), structure);
        return true;
    }
    
    if (structure->isDictionary())
        return false;
    
    // FIXME: optimize getters and setters
    if (slot.cachedPropertyType() != PropertySlot::Value)
        return false;
    
    size_t offset = slot.cachedOffset();
    size_t count = normalizePrototypeChain(exec, baseValue, slot.slotBase(), propertyName, offset);
    if (!count)
        return false;

    StructureChain* prototypeChain = structure->prototypeChain(exec);
    
    ASSERT(slot.slotBase().isObject());
    
    generateProtoChainAccessStub(exec, stubInfo, prototypeChain, count, offset, structure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase), stubInfo.stubRoutine);
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), CodeLocationLabel(stubInfo.stubRoutine.code()));
    repatchBuffer.relink(stubInfo.callReturnLocation, operationGetByIdProtoBuildList);
    
    stubInfo.initGetByIdChain(*globalData, codeBlock->ownerExecutable(), structure, prototypeChain);
    return true;
}

void dfgRepatchGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    bool cached = tryCacheGetByID(exec, baseValue, propertyName, slot, stubInfo);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
}

static bool tryBuildGetByIDList(ExecState* exec, JSValue baseValue, const Identifier& ident, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (!baseValue.isCell()
        || !slot.isCacheable()
        || baseValue.asCell()->structure()->isUncacheableDictionary()
        || slot.slotBase() != baseValue)
        return false;
    
    if (!stubInfo.patch.dfg.registersFlushed) {
        // We cannot do as much inline caching if the registers were not flushed prior to this GetById. In particular,
        // non-Value cached properties require planting calls, which requires registers to have been flushed. Thus,
        // if registers were not flushed, don't do non-Value caching.
        if (slot.cachedPropertyType() != PropertySlot::Value)
            return false;
    }
    
    CodeBlock* codeBlock = exec->codeBlock();
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    JSGlobalData* globalData = &exec->globalData();
    
    ASSERT(slot.slotBase().isObject());
    
    PolymorphicAccessStructureList* polymorphicStructureList;
    int listIndex;
    
    if (stubInfo.accessType == access_unset) {
        ASSERT(!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList();
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 0);
        listIndex = 0;
    } else if (stubInfo.accessType == access_get_by_id_self) {
        ASSERT(!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList(*globalData, codeBlock->ownerExecutable(), MacroAssemblerCodeRef::createSelfManagedCodeRef(stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase)), stubInfo.u.getByIdSelf.baseObjectStructure.get(), true);
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 1);
        listIndex = 1;
    } else {
        polymorphicStructureList = stubInfo.u.getByIdSelfList.structureList;
        listIndex = stubInfo.u.getByIdSelfList.listSize;
    }
    
    if (listIndex < POLYMORPHIC_LIST_CACHE_SIZE) {
        stubInfo.u.getByIdSelfList.listSize++;
        
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.dfg.baseGPR);
#if USE(JSVALUE32_64)
        GPRReg resultTagGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueTagGPR);
#endif
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueGPR);
        GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.patch.dfg.scratchGPR);
        
        CCallHelpers stubJit(globalData, codeBlock);
        
        MacroAssembler::Jump wrongStruct = stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(structure));
        
        // The strategy we use for stubs is as follows:
        // 1) Call DFG helper that calls the getter.
        // 2) Check if there was an exception, and if there was, call yet another
        //    helper.
        
        bool isDirect = false;
        MacroAssembler::Call operationCall;
        MacroAssembler::Call handlerCall;
        FunctionPtr operationFunction;
        MacroAssembler::Jump success;
        
        if (slot.cachedPropertyType() == PropertySlot::Getter
            || slot.cachedPropertyType() == PropertySlot::Custom) {
            if (slot.cachedPropertyType() == PropertySlot::Getter) {
                ASSERT(baseGPR != scratchGPR);
                stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
#if USE(JSVALUE64)
                stubJit.loadPtr(MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue)), scratchGPR);
#elif USE(JSVALUE32_64)
                stubJit.load32(MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), scratchGPR);
#endif
                stubJit.setupArgumentsWithExecState(baseGPR, scratchGPR);
                operationFunction = operationCallGetter;
            } else {
                stubJit.setupArgumentsWithExecState(
                    baseGPR,
                    MacroAssembler::TrustedImmPtr(FunctionPtr(slot.customGetter()).executableAddress()),
                    MacroAssembler::TrustedImmPtr(const_cast<Identifier*>(&ident)));
                operationFunction = operationCallCustomGetter;
            }
            
            // Need to make sure that whenever this call is made in the future, we remember the
            // place that we made it from. It just so happens to be the place that we are at
            // right now!
            stubJit.store32(
                MacroAssembler::TrustedImm32(exec->codeOriginIndexForDFG()),
                CCallHelpers::tagFor(static_cast<VirtualRegister>(RegisterFile::ArgumentCount)));
            
            operationCall = stubJit.call();
#if USE(JSVALUE64)
            stubJit.move(GPRInfo::returnValueGPR, resultGPR);
#else
            stubJit.setupResults(resultGPR, resultTagGPR);
#endif
            success = stubJit.emitExceptionCheck(CCallHelpers::InvertedExceptionCheck);
            
            stubJit.setupArgumentsWithExecState(
                MacroAssembler::TrustedImmPtr(&stubInfo));
            handlerCall = stubJit.call();
            stubJit.jump(GPRInfo::returnValueGPR2);
        } else {
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), resultGPR);
#if USE(JSVALUE64)
            stubJit.loadPtr(MacroAssembler::Address(resultGPR, slot.cachedOffset() * sizeof(JSValue)), resultGPR);
#elif USE(JSVALUE32_64)
            stubJit.load32(MacroAssembler::Address(resultGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)), resultTagGPR);
            stubJit.load32(MacroAssembler::Address(resultGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)), resultGPR);
#endif
            success = stubJit.jump();
            isDirect = true;
        }

        LinkBuffer patchBuffer(*globalData, &stubJit, codeBlock);
        
        CodeLocationLabel lastProtoBegin;
        if (listIndex)
            lastProtoBegin = CodeLocationLabel(polymorphicStructureList->list[listIndex - 1].stubRoutine.code());
        else
            lastProtoBegin = stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase);
        ASSERT(!!lastProtoBegin);
        
        patchBuffer.link(wrongStruct, lastProtoBegin);
        patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone));
        if (!isDirect) {
            patchBuffer.link(operationCall, operationFunction);
            patchBuffer.link(handlerCall, lookupExceptionHandlerInStub);
        }
        
        MacroAssemblerCodeRef stubRoutine = patchBuffer.finalizeCode();
        
        polymorphicStructureList->list[listIndex].set(*globalData, codeBlock->ownerExecutable(), stubRoutine, structure, isDirect);
        
        CodeLocationJump jumpLocation = stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck);
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubRoutine.code()));
        
        if (listIndex < (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            return true;
    }
    
    return false;
}

void dfgBuildGetByIDList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    bool dontChangeCall = tryBuildGetByIDList(exec, baseValue, propertyName, slot, stubInfo);
    if (!dontChangeCall)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
}

static bool tryBuildGetByIDProtoList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (!baseValue.isCell()
        || !slot.isCacheable()
        || baseValue.asCell()->structure()->isDictionary()
        || baseValue.asCell()->structure()->typeInfo().prohibitsPropertyCaching()
        || slot.slotBase() == baseValue
        || slot.cachedPropertyType() != PropertySlot::Value)
        return false;
    
    ASSERT(slot.slotBase().isObject());
    
    size_t offset = slot.cachedOffset();
    size_t count = normalizePrototypeChain(exec, baseValue, slot.slotBase(), propertyName, offset);
    if (!count)
        return false;

    Structure* structure = baseValue.asCell()->structure();
    StructureChain* prototypeChain = structure->prototypeChain(exec);
    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();
    
    PolymorphicAccessStructureList* polymorphicStructureList;
    int listIndex = 1;
    
    if (stubInfo.accessType == access_get_by_id_chain) {
        ASSERT(!!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList(*globalData, codeBlock->ownerExecutable(), stubInfo.stubRoutine, stubInfo.u.getByIdChain.baseObjectStructure.get(), stubInfo.u.getByIdChain.chain.get(), true);
        stubInfo.stubRoutine = MacroAssemblerCodeRef();
        stubInfo.initGetByIdProtoList(polymorphicStructureList, 1);
    } else {
        ASSERT(stubInfo.accessType == access_get_by_id_proto_list);
        polymorphicStructureList = stubInfo.u.getByIdProtoList.structureList;
        listIndex = stubInfo.u.getByIdProtoList.listSize;
    }
    
    if (listIndex < POLYMORPHIC_LIST_CACHE_SIZE) {
        stubInfo.u.getByIdProtoList.listSize++;
        
        CodeLocationLabel lastProtoBegin = CodeLocationLabel(polymorphicStructureList->list[listIndex - 1].stubRoutine.code());
        ASSERT(!!lastProtoBegin);

        MacroAssemblerCodeRef stubRoutine;
        
        generateProtoChainAccessStub(exec, stubInfo, prototypeChain, count, offset, structure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone), lastProtoBegin, stubRoutine);
        
        polymorphicStructureList->list[listIndex].set(*globalData, codeBlock->ownerExecutable(), stubRoutine, structure, true);
        
        CodeLocationJump jumpLocation = stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck);
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(jumpLocation, CodeLocationLabel(stubRoutine.code()));
        
        if (listIndex < (POLYMORPHIC_LIST_CACHE_SIZE - 1))
            return true;
    }
    
    return false;
}

void dfgBuildGetByIDProtoList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    bool dontChangeCall = tryBuildGetByIDProtoList(exec, baseValue, propertyName, slot, stubInfo);
    if (!dontChangeCall)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, operationGetById);
}

static V_DFGOperation_EJCI appropriateGenericPutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
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

static V_DFGOperation_EJCI appropriateListBuildingPutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
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

static void testPrototype(MacroAssembler &stubJit, GPRReg scratchGPR, JSValue prototype, MacroAssembler::JumpList& failureCases)
{
    if (prototype.isNull())
        return;
    
    ASSERT(prototype.isCell());
    
    stubJit.move(MacroAssembler::TrustedImmPtr(prototype.asCell()), scratchGPR);
    failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(scratchGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(prototype.asCell()->structure())));
}

static void emitPutReplaceStub(
    ExecState* exec,
    JSValue,
    const Identifier&,
    const PutPropertySlot& slot,
    StructureStubInfo& stubInfo,
    PutKind,
    Structure* structure,
    CodeLocationLabel failureLabel,
    MacroAssemblerCodeRef& stubRoutine)
{
    JSGlobalData* globalData = &exec->globalData();
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.dfg.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg valueTagGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueTagGPR);
#endif
    GPRReg valueGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueGPR);
    GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.patch.dfg.scratchGPR);
    bool needToRestoreScratch = false;
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
    GPRReg scratchGPR2;
    const bool writeBarrierNeeded = true;
#else
    const bool writeBarrierNeeded = false;
#endif
    
    MacroAssembler stubJit;
    
    if (scratchGPR == InvalidGPRReg && (writeBarrierNeeded || !structure->isUsingInlineStorage())) {
        scratchGPR = SpeculativeJIT::selectScratchGPR(baseGPR, valueGPR);
        needToRestoreScratch = true;
        stubJit.push(scratchGPR);
    }

    MacroAssembler::Jump badStructure = stubJit.branchPtr(
        MacroAssembler::NotEqual,
        MacroAssembler::Address(baseGPR, JSCell::structureOffset()),
        MacroAssembler::TrustedImmPtr(structure));
    
#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
    scratchGPR2 = SpeculativeJIT::selectScratchGPR(baseGPR, valueGPR, scratchGPR);
    stubJit.push(scratchGPR2);
    SpeculativeJIT::writeBarrier(stubJit, baseGPR, scratchGPR, scratchGPR2, WriteBarrierForPropertyAccess);
    stubJit.pop(scratchGPR2);
#endif
    
#if USE(JSVALUE64)
    if (structure->isUsingInlineStorage())
        stubJit.storePtr(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue)));
    else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
        stubJit.storePtr(valueGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue)));
    }
#elif USE(JSVALUE32_64)
    if (structure->isUsingInlineStorage()) {
        stubJit.store32(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    } else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
        stubJit.store32(valueGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
    
    MacroAssembler::Jump success;
    MacroAssembler::Jump failure;
    
    if (needToRestoreScratch) {
        stubJit.pop(scratchGPR);
        success = stubJit.jump();
        
        badStructure.link(&stubJit);
        stubJit.pop(scratchGPR);
        failure = stubJit.jump();
    } else {
        success = stubJit.jump();
        failure = badStructure;
    }
    
    LinkBuffer patchBuffer(*globalData, &stubJit, exec->codeBlock());
    patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone));
    patchBuffer.link(failure, failureLabel);
            
    stubRoutine = patchBuffer.finalizeCode();
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
    MacroAssemblerCodeRef& stubRoutine)
{
    JSGlobalData* globalData = &exec->globalData();

    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.patch.dfg.baseGPR);
#if USE(JSVALUE32_64)
    GPRReg valueTagGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueTagGPR);
#endif
    GPRReg valueGPR = static_cast<GPRReg>(stubInfo.patch.dfg.valueGPR);
    GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.patch.dfg.scratchGPR);
    bool needToRestoreScratch = false;
            
    ASSERT(scratchGPR != baseGPR);
            
    MacroAssembler stubJit;
            
    MacroAssembler::JumpList failureCases;
            
    if (scratchGPR == InvalidGPRReg) {
        scratchGPR = SpeculativeJIT::selectScratchGPR(baseGPR, valueGPR);
        stubJit.push(scratchGPR);
        needToRestoreScratch = true;
    }
    
    failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(oldStructure)));
            
    testPrototype(stubJit, scratchGPR, oldStructure->storedPrototype(), failureCases);
            
    if (putKind == NotDirect) {
        for (WriteBarrier<Structure>* it = prototypeChain->head(); *it; ++it)
            testPrototype(stubJit, scratchGPR, (*it)->storedPrototype(), failureCases);
    }

#if ENABLE(GGC) || ENABLE(WRITE_BARRIER_PROFILING)
    // Must always emit this write barrier as the structure transition itself requires it
    GPRReg scratch2 = SpeculativeJIT::selectScratchGPR(baseGPR, valueGPR, scratchGPR);
    stubJit.push(scratch2);
    SpeculativeJIT::writeBarrier(stubJit, baseGPR, scratchGPR, scratch2, WriteBarrierForPropertyAccess);
    stubJit.pop(scratch2);
#endif

    stubJit.storePtr(MacroAssembler::TrustedImmPtr(structure), MacroAssembler::Address(baseGPR, JSCell::structureOffset()));
#if USE(JSVALUE64)
    if (structure->isUsingInlineStorage())
        stubJit.storePtr(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue)));
    else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
        stubJit.storePtr(valueGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue)));
    }
#elif USE(JSVALUE32_64)
    if (structure->isUsingInlineStorage()) {
        stubJit.store32(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    } else {
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
        stubJit.store32(valueGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
        stubJit.store32(valueTagGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue) + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
            
    MacroAssembler::Jump success;
    MacroAssembler::Jump failure;
            
    if (needToRestoreScratch) {
        stubJit.pop(scratchGPR);
        success = stubJit.jump();

        failureCases.link(&stubJit);
        stubJit.pop(scratchGPR);
        failure = stubJit.jump();
    } else
        success = stubJit.jump();
            
    LinkBuffer patchBuffer(*globalData, &stubJit, exec->codeBlock());
    patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToDone));
    if (needToRestoreScratch)
        patchBuffer.link(failure, failureLabel);
    else
        patchBuffer.link(failureCases, failureLabel);
            
    stubRoutine = patchBuffer.finalizeCode();
}

static bool tryCachePutByID(ExecState* exec, JSValue baseValue, const Identifier& ident, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();

    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    Structure* oldStructure = structure->previousID();
    
    if (!slot.isCacheable())
        return false;
    if (structure->isUncacheableDictionary())
        return false;

    // Optimize self access.
    if (slot.base() == baseValue) {
        if (slot.type() == PutPropertySlot::NewProperty) {
            if (structure->isDictionary())
                return false;
            
            // skip optimizing the case where we need a realloc
            if (oldStructure->propertyStorageCapacity() != structure->propertyStorageCapacity())
                return false;
            
            normalizePrototypeChain(exec, baseCell);
            
            StructureChain* prototypeChain = structure->prototypeChain(exec);
            
            emitPutTransitionStub(
                exec, baseValue, ident, slot, stubInfo, putKind,
                structure, oldStructure, prototypeChain,
                stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase),
                stubInfo.stubRoutine);
            
            RepatchBuffer repatchBuffer(codeBlock);
            repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), CodeLocationLabel(stubInfo.stubRoutine.code()));
            repatchBuffer.relink(stubInfo.callReturnLocation, appropriateListBuildingPutByIdFunction(slot, putKind));
            
            stubInfo.initPutByIdTransition(*globalData, codeBlock->ownerExecutable(), oldStructure, structure, prototypeChain, putKind == Direct);
            
            return true;
        }

        dfgRepatchByIdSelfAccess(codeBlock, stubInfo, structure, slot.cachedOffset(), appropriateListBuildingPutByIdFunction(slot, putKind), false);
        stubInfo.initPutByIdReplace(*globalData, codeBlock->ownerExecutable(), structure);
        return true;
    }

    return false;
}

void dfgRepatchPutByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    bool cached = tryCachePutByID(exec, baseValue, propertyName, slot, stubInfo, putKind);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

static bool tryBuildPutByIdList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();

    if (!baseValue.isCell())
        return false;
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    Structure* oldStructure = structure->previousID();
    
    if (!slot.isCacheable())
        return false;
    if (structure->isUncacheableDictionary())
        return false;

    // Optimize self access.
    if (slot.base() == baseValue) {
        PolymorphicPutByIdList* list;
        MacroAssemblerCodeRef stubRoutine;
        
        if (slot.type() == PutPropertySlot::NewProperty) {
            if (structure->isDictionary())
                return false;
            
            // skip optimizing the case where we need a realloc
            if (oldStructure->propertyStorageCapacity() != structure->propertyStorageCapacity())
                return false;
            
            normalizePrototypeChain(exec, baseCell);
            
            StructureChain* prototypeChain = structure->prototypeChain(exec);
            
            // We're now committed to creating the stub. Mogrify the meta-data accordingly.
            list = PolymorphicPutByIdList::from(
                putKind, stubInfo,
                stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase));
            
            emitPutTransitionStub(
                exec, baseValue, propertyName, slot, stubInfo, putKind,
                structure, oldStructure, prototypeChain,
                CodeLocationLabel(list->currentSlowPathTarget()),
                stubRoutine);
            
            list->addAccess(
                PutByIdAccess::transition(
                    *globalData, codeBlock->ownerExecutable(),
                    oldStructure, structure, prototypeChain,
                    stubRoutine));
        } else {
            // We're now committed to creating the stub. Mogrify the meta-data accordingly.
            list = PolymorphicPutByIdList::from(
                putKind, stubInfo,
                stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase));
            
            emitPutReplaceStub(
                exec, baseValue, propertyName, slot, stubInfo, putKind,
                structure, CodeLocationLabel(list->currentSlowPathTarget()), stubRoutine);
            
            list->addAccess(
                PutByIdAccess::replace(
                    *globalData, codeBlock->ownerExecutable(),
                    structure, stubRoutine));
        }
        
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), CodeLocationLabel(stubRoutine.code()));
        
        if (list->isFull())
            repatchBuffer.relink(stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
        
        return true;
    }
    
    return false;
}

void dfgBuildPutByIdList(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    bool cached = tryBuildPutByIdList(exec, baseValue, propertyName, slot, stubInfo, putKind);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

void dfgLinkFor(ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock, JSFunction* callee, MacroAssemblerCodePtr codePtr, CodeSpecializationKind kind)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    
    RepatchBuffer repatchBuffer(callerCodeBlock);
    
    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.callee.set(exec->callerFrame()->globalData(), callLinkInfo.hotPathBegin, callerCodeBlock->ownerExecutable(), callee);
    callLinkInfo.lastSeenCallee.set(exec->callerFrame()->globalData(), callerCodeBlock->ownerExecutable(), callee);
    repatchBuffer.relink(callLinkInfo.hotPathOther, codePtr);
    
    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(&callLinkInfo);
    
    if (kind == CodeForCall) {
        repatchBuffer.relink(CodeLocationCall(callLinkInfo.callReturnLocation), operationVirtualCall);
        return;
    }
    ASSERT(kind == CodeForConstruct);
    repatchBuffer.relink(CodeLocationCall(callLinkInfo.callReturnLocation), operationVirtualConstruct);
}

void dfgResetGetByID(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    repatchBuffer.relink(stubInfo.callReturnLocation, operationGetByIdOptimize);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelPtrAtOffset(-(uintptr_t)stubInfo.patch.dfg.deltaCheckImmToCall), reinterpret_cast<void*>(-1));
#if USE(JSVALUE64)
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToLoadOrStore), 0);
#else
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToTagLoadOrStore), 0);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.patch.dfg.deltaCallToPayloadLoadOrStore), 0);
#endif
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase));
}

void dfgResetPutByID(RepatchBuffer& repatchBuffer, StructureStubInfo& stubInfo)
{
    V_DFGOperation_EJCI unoptimizedFunction = bitwise_cast<V_DFGOperation_EJCI>(MacroAssembler::readCallTarget(stubInfo.callReturnLocation).executableAddress());
    V_DFGOperation_EJCI optimizedFunction;
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
    repatchBuffer.relink(stubInfo.callReturnLocation, optimizedFunction);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelPtrAtOffset(-(uintptr_t)stubInfo.patch.dfg.deltaCheckImmToCall), reinterpret_cast<void*>(-1));
#if USE(JSVALUE64)
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToLoadOrStore), 0);
#else
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToTagLoadOrStore), 0);
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.patch.dfg.deltaCallToPayloadLoadOrStore), 0);
#endif
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.patch.dfg.deltaCallToStructCheck), stubInfo.callReturnLocation.labelAtOffset(stubInfo.patch.dfg.deltaCallToSlowCase));
}

} } // namespace JSC::DFG

#endif
