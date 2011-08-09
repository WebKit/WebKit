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

#include "DFGJITCodeGenerator.h"
#include "LinkBuffer.h"
#include "Operations.h"
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
    repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelPtrAtOffset(-(intptr_t)stubInfo.u.unset.deltaCheckImmToCall), structure);
    if (compact)
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabelCompactAtOffset(stubInfo.u.unset.deltaCallToLoadOrStore), sizeof(JSValue) * offset);
    else
        repatchBuffer.repatch(stubInfo.callReturnLocation.dataLabel32AtOffset(stubInfo.u.unset.deltaCallToLoadOrStore), sizeof(JSValue) * offset);
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
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToDone), stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToSlowCase));
}

static void generateProtoChainAccessStub(ExecState* exec, StructureStubInfo& stubInfo, StructureChain* chain, size_t count, size_t offset, Structure* structure, CodeLocationLabel successLabel, CodeLocationLabel slowCaseLabel, CodeLocationLabel& stubRoutine)
{
    JSGlobalData* globalData = &exec->globalData();

    MacroAssembler stubJit;
        
    GPRReg baseGPR = static_cast<GPRReg>(stubInfo.baseGPR);
    GPRReg resultGPR = static_cast<GPRReg>(stubInfo.valueGPR);
    GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.scratchGPR);
    bool needToRestoreScratch = false;
    
    if (scratchGPR == InvalidGPRReg) {
        scratchGPR = JITCodeGenerator::selectScratchGPR(baseGPR, resultGPR);
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
    
    if (protoObject->structure()->isUsingInlineStorage())
        stubJit.loadPtr(MacroAssembler::Address(scratchGPR, JSObject::offsetOfInlineStorage() + offset * sizeof(JSValue)), resultGPR);
    else
        stubJit.loadPtr(protoObject->addressOfPropertyAtOffset(offset), resultGPR);
        
    MacroAssembler::Jump success, fail;
    
    emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
    
    LinkBuffer patchBuffer(*globalData, &stubJit, exec->codeBlock()->executablePool());
    
    linkRestoreScratch(patchBuffer, needToRestoreScratch, success, fail, failureCases, successLabel, slowCaseLabel);
    
    stubRoutine = patchBuffer.finalizeCodeAddendum();
}

static bool tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    // FIXME: Write a test that proves we need to check for recursion here just
    // like the interpreter does, then add a check for recursion.

    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();
    
    if (isJSArray(globalData, baseValue) && propertyName == exec->propertyNames().length) {
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.baseGPR);
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.valueGPR);
        GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.scratchGPR);
        bool needToRestoreScratch = false;
        
        MacroAssembler stubJit;
        
        if (scratchGPR == InvalidGPRReg) {
            scratchGPR = JITCodeGenerator::selectScratchGPR(baseGPR, resultGPR);
            stubJit.push(scratchGPR);
            needToRestoreScratch = true;
        }
        
        MacroAssembler::JumpList failureCases;
        
        failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR), MacroAssembler::TrustedImmPtr(globalData->jsArrayVPtr)));
        
        stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSArray::storageOffset()), scratchGPR);
        stubJit.load32(MacroAssembler::Address(scratchGPR, OBJECT_OFFSETOF(ArrayStorage, m_length)), scratchGPR);
        failureCases.append(stubJit.branch32(MacroAssembler::LessThan, scratchGPR, MacroAssembler::TrustedImm32(0)));
        
        stubJit.orPtr(GPRInfo::tagTypeNumberRegister, scratchGPR, resultGPR);

        MacroAssembler::Jump success, fail;
        
        emitRestoreScratch(stubJit, needToRestoreScratch, scratchGPR, success, fail, failureCases);
        
        LinkBuffer patchBuffer(*globalData, &stubJit, codeBlock->executablePool());
        
        linkRestoreScratch(patchBuffer, needToRestoreScratch, stubInfo, success, fail, failureCases);
        
        CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
        stubInfo.stubRoutine = entryLabel;
        
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.deltaCallToStructCheck), entryLabel);
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
        if ((slot.cachedPropertyType() != PropertySlot::Value) || ((slot.cachedOffset() * sizeof(JSValue)) > (unsigned)MacroAssembler::MaximumCompactPtrAlignedAddressOffset))
            return false;

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
    
    generateProtoChainAccessStub(exec, stubInfo, prototypeChain, count, offset, structure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToDone), stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToSlowCase), stubInfo.stubRoutine);
    
    RepatchBuffer repatchBuffer(codeBlock);
    repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.deltaCallToStructCheck), stubInfo.stubRoutine);
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

static void dfgRepatchGetMethodFast(JSGlobalData* globalData, CodeBlock* codeBlock, MethodCallLinkInfo& methodInfo, JSObjectWithGlobalObject* callee, Structure* structure, JSObject* slotBaseObject)
{
    ScriptExecutable* owner = codeBlock->ownerExecutable();
    
    RepatchBuffer repatchBuffer(codeBlock);

    // Only optimize once!
    repatchBuffer.relink(methodInfo.callReturnLocation, operationGetByIdOptimize);

    methodInfo.cachedStructure.set(*globalData, owner, structure);
    methodInfo.cachedPrototypeStructure.set(*globalData, owner, slotBaseObject->structure());
    methodInfo.cachedPrototype.set(*globalData, owner, slotBaseObject);
    methodInfo.cachedFunction.set(*globalData, owner, callee);
}

static bool tryCacheGetMethod(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, MethodCallLinkInfo& methodInfo)
{
    CodeBlock* codeBlock = exec->codeBlock();
    JSGlobalData* globalData = &exec->globalData();
    
    Structure* structure;
    JSCell* specific;
    JSObject* slotBaseObject;
    if (baseValue.isCell()
        && slot.isCacheableValue()
        && !(structure = baseValue.asCell()->structure())->isUncacheableDictionary()
        && (slotBaseObject = asObject(slot.slotBase()))->getPropertySpecificValue(exec, propertyName, specific)
        && specific) {
        
        JSObjectWithGlobalObject* callee = (JSObjectWithGlobalObject*)specific;
        
        // Since we're accessing a prototype in a loop, it's a good bet that it
        // should not be treated as a dictionary.
        if (slotBaseObject->structure()->isDictionary())
            slotBaseObject->flattenDictionaryObject(exec->globalData());
        
        if (slot.slotBase() == structure->prototypeForLookup(exec)) {
            dfgRepatchGetMethodFast(globalData, codeBlock, methodInfo, callee, structure, slotBaseObject);
            return true;
        }
        
        if (slot.slotBase() == baseValue) {
            dfgRepatchGetMethodFast(globalData, codeBlock, methodInfo, callee, structure, exec->scopeChain()->globalObject->methodCallDummy());
            return true;
        }
    }
    
    return false;
}

void dfgRepatchGetMethod(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, MethodCallLinkInfo& methodInfo)
{
    bool cached = tryCacheGetMethod(exec, baseValue, propertyName, slot, methodInfo);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), methodInfo.callReturnLocation, operationGetByIdOptimize);
}

static bool tryBuildGetByIDList(ExecState* exec, JSValue baseValue, const Identifier&, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    if (!baseValue.isCell()
        || !slot.isCacheable()
        || baseValue.asCell()->structure()->isUncacheableDictionary()
        || slot.slotBase() != baseValue
        || slot.cachedPropertyType() != PropertySlot::Value
        || (slot.cachedOffset() * sizeof(JSValue)) > (unsigned)MacroAssembler::MaximumCompactPtrAlignedAddressOffset)
        return false;
    
    CodeBlock* codeBlock = exec->codeBlock();
    JSCell* baseCell = baseValue.asCell();
    Structure* structure = baseCell->structure();
    JSGlobalData* globalData = &exec->globalData();
    
    ASSERT(slot.slotBase().isObject());
    
    PolymorphicAccessStructureList* polymorphicStructureList;
    int listIndex = 1;
    
    if (stubInfo.accessType == access_get_by_id_self) {
        ASSERT(!stubInfo.stubRoutine);
        polymorphicStructureList = new PolymorphicAccessStructureList(*globalData, codeBlock->ownerExecutable(), stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToSlowCase), stubInfo.u.getByIdSelf.baseObjectStructure.get());
        stubInfo.initGetByIdSelfList(polymorphicStructureList, 1);
    } else {
        polymorphicStructureList = stubInfo.u.getByIdSelfList.structureList;
        listIndex = stubInfo.u.getByIdSelfList.listSize;
    }
    
    if (listIndex < POLYMORPHIC_LIST_CACHE_SIZE) {
        stubInfo.u.getByIdSelfList.listSize++;
        
        GPRReg baseGPR = static_cast<GPRReg>(stubInfo.baseGPR);
        GPRReg resultGPR = static_cast<GPRReg>(stubInfo.valueGPR);
        
        MacroAssembler stubJit;
        
        MacroAssembler::Jump wrongStruct = stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(structure));
        
        if (structure->isUsingInlineStorage())
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue)), resultGPR);
        else {
            stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), resultGPR);
            stubJit.loadPtr(MacroAssembler::Address(resultGPR, slot.cachedOffset() * sizeof(JSValue)), resultGPR);
        }
        
        MacroAssembler::Jump success = stubJit.jump();
        
        LinkBuffer patchBuffer(*globalData, &stubJit, codeBlock->executablePool());
        
        CodeLocationLabel lastProtoBegin = polymorphicStructureList->list[listIndex - 1].stubRoutine;
        ASSERT(!!lastProtoBegin);
        
        patchBuffer.link(wrongStruct, lastProtoBegin);
        patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToDone));
        
        CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
        
        polymorphicStructureList->list[listIndex].set(*globalData, codeBlock->ownerExecutable(), entryLabel, structure);
        
        CodeLocationJump jumpLocation = stubInfo.callReturnLocation.jumpAtOffset(stubInfo.deltaCallToStructCheck);
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(jumpLocation, entryLabel);
        
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
        polymorphicStructureList = new PolymorphicAccessStructureList(*globalData, codeBlock->ownerExecutable(), stubInfo.stubRoutine, stubInfo.u.getByIdChain.baseObjectStructure.get(), stubInfo.u.getByIdChain.chain.get());
        stubInfo.stubRoutine = CodeLocationLabel();
        stubInfo.initGetByIdProtoList(polymorphicStructureList, 1);
    } else {
        ASSERT(stubInfo.accessType = access_get_by_id_proto_list);
        polymorphicStructureList = stubInfo.u.getByIdProtoList.structureList;
        listIndex = stubInfo.u.getByIdProtoList.listSize;
    }
    
    if (listIndex < POLYMORPHIC_LIST_CACHE_SIZE) {
        stubInfo.u.getByIdProtoList.listSize++;
        
        CodeLocationLabel lastProtoBegin = polymorphicStructureList->list[listIndex - 1].stubRoutine;
        ASSERT(!!lastProtoBegin);

        CodeLocationLabel entryLabel;
        
        generateProtoChainAccessStub(exec, stubInfo, prototypeChain, count, offset, structure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToDone), lastProtoBegin, entryLabel);
        
        polymorphicStructureList->list[listIndex].set(*globalData, codeBlock->ownerExecutable(), entryLabel, structure);
        
        CodeLocationJump jumpLocation = stubInfo.callReturnLocation.jumpAtOffset(stubInfo.deltaCallToStructCheck);
        RepatchBuffer repatchBuffer(codeBlock);
        repatchBuffer.relink(jumpLocation, entryLabel);
        
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

static V_DFGOperation_EJJI appropriatePutByIdFunction(const PutPropertySlot &slot, PutKind putKind)
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

static void testPrototype(MacroAssembler &stubJit, GPRReg scratchGPR, JSValue prototype, MacroAssembler::JumpList& failureCases)
{
    if (prototype.isNull())
        return;
    
    ASSERT(prototype.isCell());
    
    stubJit.move(MacroAssembler::TrustedImmPtr(prototype.asCell()), scratchGPR);
    failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(scratchGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(prototype.asCell()->structure())));
}

static bool tryCachePutByID(ExecState* exec, JSValue baseValue, const Identifier&, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
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
            
            GPRReg baseGPR = static_cast<GPRReg>(stubInfo.baseGPR);
            GPRReg valueGPR = static_cast<GPRReg>(stubInfo.valueGPR);
            GPRReg scratchGPR = static_cast<GPRReg>(stubInfo.scratchGPR);
            bool needToRestoreScratch = false;
            
            ASSERT(scratchGPR != baseGPR);
            
            MacroAssembler stubJit;
            
            MacroAssembler::JumpList failureCases;
            
            if (scratchGPR == InvalidGPRReg) {
                scratchGPR = JITCodeGenerator::selectScratchGPR(baseGPR, valueGPR);
                stubJit.push(scratchGPR);
                needToRestoreScratch = true;
            }
            
            failureCases.append(stubJit.branchPtr(MacroAssembler::NotEqual, MacroAssembler::Address(baseGPR, JSCell::structureOffset()), MacroAssembler::TrustedImmPtr(oldStructure)));
            
            testPrototype(stubJit, scratchGPR, oldStructure->storedPrototype(), failureCases);
            
            if (putKind == NotDirect) {
                for (WriteBarrier<Structure>* it = prototypeChain->head(); *it; ++it)
                    testPrototype(stubJit, scratchGPR, (*it)->storedPrototype(), failureCases);
            }
            
            JITCodeGenerator::writeBarrier(stubJit, baseGPR, scratchGPR);
            
            stubJit.storePtr(MacroAssembler::TrustedImmPtr(structure), MacroAssembler::Address(baseGPR, JSCell::structureOffset()));
            if (structure->isUsingInlineStorage())
                stubJit.storePtr(valueGPR, MacroAssembler::Address(baseGPR, JSObject::offsetOfInlineStorage() + slot.cachedOffset() * sizeof(JSValue)));
            else {
                stubJit.loadPtr(MacroAssembler::Address(baseGPR, JSObject::offsetOfPropertyStorage()), scratchGPR);
                stubJit.storePtr(valueGPR, MacroAssembler::Address(scratchGPR, slot.cachedOffset() * sizeof(JSValue)));
            }
            
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
            
            LinkBuffer patchBuffer(*globalData, &stubJit, codeBlock->executablePool());
            patchBuffer.link(success, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToDone));
            if (needToRestoreScratch)
                patchBuffer.link(failure, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToSlowCase));
            else
                patchBuffer.link(failureCases, stubInfo.callReturnLocation.labelAtOffset(stubInfo.deltaCallToSlowCase));
            
            CodeLocationLabel entryLabel = patchBuffer.finalizeCodeAddendum();
            stubInfo.stubRoutine = entryLabel;
            
            RepatchBuffer repatchBuffer(codeBlock);
            repatchBuffer.relink(stubInfo.callReturnLocation.jumpAtOffset(stubInfo.deltaCallToStructCheck), entryLabel);
            repatchBuffer.relink(stubInfo.callReturnLocation, appropriatePutByIdFunction(slot, putKind));
            
            stubInfo.initPutByIdTransition(*globalData, codeBlock->ownerExecutable(), oldStructure, structure, prototypeChain);
            
            return true;
        }

        dfgRepatchByIdSelfAccess(codeBlock, stubInfo, structure, slot.cachedOffset(), appropriatePutByIdFunction(slot, putKind), false);
        stubInfo.initPutByIdReplace(*globalData, codeBlock->ownerExecutable(), structure);
        return true;
    }

    // FIXME: should support the transition case!
    return false;
}

void dfgRepatchPutByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    bool cached = tryCachePutByID(exec, baseValue, propertyName, slot, stubInfo, putKind);
    if (!cached)
        dfgRepatchCall(exec->codeBlock(), stubInfo.callReturnLocation, appropriatePutByIdFunction(slot, putKind));
}

void dfgLinkFor(ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock, JSFunction* callee, MacroAssemblerCodePtr codePtr, CodeSpecializationKind kind)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    
    RepatchBuffer repatchBuffer(callerCodeBlock);
    
    if (!calleeCodeBlock || static_cast<int>(exec->argumentCountIncludingThis()) == calleeCodeBlock->m_numParameters) {
        ASSERT(!callLinkInfo.isLinked());
        callLinkInfo.callee.set(exec->callerFrame()->globalData(), callLinkInfo.hotPathBegin, callerCodeBlock->ownerExecutable(), callee);
        repatchBuffer.relink(callLinkInfo.hotPathOther, codePtr);
    }
    
    if (kind == CodeForCall) {
        repatchBuffer.relink(CodeLocationCall(callLinkInfo.callReturnLocation), operationVirtualCall);
        return;
    }
    ASSERT(kind == CodeForConstruct);
    repatchBuffer.relink(CodeLocationCall(callLinkInfo.callReturnLocation), operationVirtualConstruct);
}

} } // namespace JSC::DFG

#endif
