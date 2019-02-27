/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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
#include "DOMJITGetterSetter.h"
#include "DirectArguments.h"
#include "ExecutableBaseInlines.h"
#include "FTLThunks.h"
#include "FullCodeOrigin.h"
#include "FunctionCodeBlock.h"
#include "GCAwareJITStubRoutine.h"
#include "GetterSetter.h"
#include "GetterSetterAccessCase.h"
#include "ICStats.h"
#include "InlineAccess.h"
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JIT.h"
#include "JITInlines.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssembly.h"
#include "LinkBuffer.h"
#include "ModuleNamespaceAccessCase.h"
#include "PolymorphicAccess.h"
#include "ScopedArguments.h"
#include "ScratchRegisterAllocator.h"
#include "StackAlignment.h"
#include "StructureRareDataInlines.h"
#include "StructureStubClearingWatchpoint.h"
#include "StructureStubInfo.h"
#include "SuperSampler.h"
#include "ThunkGenerators.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

static FunctionPtr<CFunctionPtrTag> readPutICCallTarget(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call)
{
    FunctionPtr<OperationPtrTag> target = MacroAssembler::readCallTarget<OperationPtrTag>(call);
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITCode::FTLJIT) {
        MacroAssemblerCodePtr<JITThunkPtrTag> thunk = MacroAssemblerCodePtr<OperationPtrTag>::createFromExecutableAddress(target.executableAddress()).retagged<JITThunkPtrTag>();
        return codeBlock->vm()->ftlThunks->keyForSlowPathCallThunk(thunk).callTarget().retagged<CFunctionPtrTag>();
    }
#else
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    return target.retagged<CFunctionPtrTag>();
}

void ftlThunkAwareRepatchCall(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITCode::FTLJIT) {
        VM& vm = *codeBlock->vm();
        FTL::Thunks& thunks = *vm.ftlThunks;
        FunctionPtr<OperationPtrTag> target = MacroAssembler::readCallTarget<OperationPtrTag>(call);
        auto slowPathThunk = MacroAssemblerCodePtr<JITThunkPtrTag>::createFromExecutableAddress(target.retaggedExecutableAddress<JITThunkPtrTag>());
        FTL::SlowPathCallKey key = thunks.keyForSlowPathCallThunk(slowPathThunk);
        key = key.withCallTarget(newCalleeFunction);
        MacroAssembler::repatchCall(call, FunctionPtr<OperationPtrTag>(thunks.getSlowPathCallThunk(key).retaggedCode<OperationPtrTag>()));
        return;
    }
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    MacroAssembler::repatchCall(call, newCalleeFunction.retagged<OperationPtrTag>());
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

ALWAYS_INLINE static void fireWatchpointsAndClearStubIfNeeded(VM& vm, StructureStubInfo& stubInfo, CodeBlock* codeBlock, AccessGenerationResult& result)
{
    if (result.shouldResetStubAndFireWatchpoints()) {
        result.fireWatchpoints(vm);
        stubInfo.reset(codeBlock);
    }
}

inline FunctionPtr<CFunctionPtrTag> appropriateOptimizingGetByIdFunction(GetByIDKind kind)
{
    switch (kind) {
    case GetByIDKind::Normal:
        return operationGetByIdOptimize;
    case GetByIDKind::WithThis:
        return operationGetByIdWithThisOptimize;
    case GetByIDKind::Try:
        return operationTryGetByIdOptimize;
    case GetByIDKind::Direct:
        return operationGetByIdDirectOptimize;
    }
    ASSERT_NOT_REACHED();
    return operationGetById;
}

inline FunctionPtr<CFunctionPtrTag> appropriateGetByIdFunction(GetByIDKind kind)
{
    switch (kind) {
    case GetByIDKind::Normal:
        return operationGetById;
    case GetByIDKind::WithThis:
        return operationGetByIdWithThis;
    case GetByIDKind::Try:
        return operationTryGetById;
    case GetByIDKind::Direct:
        return operationGetByIdDirect;
    }
    ASSERT_NOT_REACHED();
    return operationGetById;
}

static InlineCacheAction tryCacheGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    VM& vm = exec->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);

        if (forceICFailure(exec))
            return GiveUpOnCache;
        
        // FIXME: Cache property access for immediates.
        if (!baseValue.isCell())
            return GiveUpOnCache;
        JSCell* baseCell = baseValue.asCell();

        CodeBlock* codeBlock = exec->codeBlock();

        std::unique_ptr<AccessCase> newCase;

        if (propertyName == vm.propertyNames->length) {
            if (isJSArray(baseCell)) {
                if (stubInfo.cacheType == CacheType::Unset
                    && slot.slotBase() == baseCell
                    && InlineAccess::isCacheableArrayLength(stubInfo, jsCast<JSArray*>(baseCell))) {

                    bool generatedCodeInline = InlineAccess::generateArrayLength(stubInfo, jsCast<JSArray*>(baseCell));
                    if (generatedCodeInline) {
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateOptimizingGetByIdFunction(kind));
                        stubInfo.initArrayLength();
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::ArrayLength);
            } else if (isJSString(baseCell)) {
                if (stubInfo.cacheType == CacheType::Unset) {
                    bool generatedCodeInline = InlineAccess::generateStringLength(stubInfo);
                    if (generatedCodeInline) {
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateOptimizingGetByIdFunction(kind));
                        stubInfo.initStringLength();
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::StringLength);
            }
            else if (DirectArguments* arguments = jsDynamicCast<DirectArguments*>(vm, baseCell)) {
                // If there were overrides, then we can handle this as a normal property load! Guarding
                // this with such a check enables us to add an IC case for that load if needed.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::DirectArgumentsLength);
            } else if (ScopedArguments* arguments = jsDynamicCast<ScopedArguments*>(vm, baseCell)) {
                // Ditto.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::ScopedArgumentsLength);
            }
        }

        if (!propertyName.isSymbol() && baseCell->inherits<JSModuleNamespaceObject>(vm) && !slot.isUnset()) {
            if (auto moduleNamespaceSlot = slot.moduleNamespaceSlot())
                newCase = ModuleNamespaceAccessCase::create(vm, codeBlock, jsCast<JSModuleNamespaceObject*>(baseCell), moduleNamespaceSlot->environment, ScopeOffset(moduleNamespaceSlot->scopeOffset));
        }
        
        if (!newCase) {
            if (!slot.isCacheable() && !slot.isUnset())
                return GiveUpOnCache;

            ObjectPropertyConditionSet conditionSet;
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
                && !structure->needImpurePropertyWatchpoint()
                && !loadTargetFromProxy) {

                bool generatedCodeInline = InlineAccess::generateSelfPropertyAccess(stubInfo, structure, slot.cachedOffset());
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::GetByIdSelfPatch, structure->classInfo(), propertyName));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateOptimizingGetByIdFunction(kind));
                    stubInfo.initGetByIdSelf(codeBlock, structure, slot.cachedOffset());
                    return RetryCacheLater;
                }
            }

            std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;

            PropertyOffset offset = slot.isUnset() ? invalidOffset : slot.cachedOffset();

            if (slot.isUnset() || slot.slotBase() != baseValue) {
                if (structure->typeInfo().prohibitsPropertyCaching())
                    return GiveUpOnCache;

                if (structure->isDictionary()) {
                    if (structure->hasBeenFlattenedBefore())
                        return GiveUpOnCache;
                    structure->flattenDictionaryStructure(vm, jsCast<JSObject*>(baseCell));
                }

                if (slot.isUnset() && structure->typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence())
                    return GiveUpOnCache;

                // If a kind is GetByIDKind::Direct, we do not need to investigate prototype chains further.
                // Cacheability just depends on the head structure.
                if (kind != GetByIDKind::Direct) {
                    bool usesPolyProto;
                    prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), baseCell, slot, usesPolyProto);
                    if (!prototypeAccessChain) {
                        // It's invalid to access this prototype property.
                        return GiveUpOnCache;
                    }

                    if (!usesPolyProto) {
                        // We use ObjectPropertyConditionSet instead for faster accesses.
                        prototypeAccessChain = nullptr;

                        // FIXME: Maybe this `if` should be inside generateConditionsForPropertyBlah.
                        // https://bugs.webkit.org/show_bug.cgi?id=185215
                        if (slot.isUnset()) {
                            conditionSet = generateConditionsForPropertyMiss(
                                vm, codeBlock, exec, structure, propertyName.impl());
                        } else if (!slot.isCacheableCustom()) {
                            conditionSet = generateConditionsForPrototypePropertyHit(
                                vm, codeBlock, exec, structure, slot.slotBase(),
                                propertyName.impl());
                        } else {
                            conditionSet = generateConditionsForPrototypePropertyHitCustom(
                                vm, codeBlock, exec, structure, slot.slotBase(),
                                propertyName.impl());
                        }

                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                }

                offset = slot.isUnset() ? invalidOffset : slot.cachedOffset();
            }

            JSFunction* getter = nullptr;
            if (slot.isCacheableGetter())
                getter = jsDynamicCast<JSFunction*>(vm, slot.getterSetter()->getter());

            Optional<DOMAttributeAnnotation> domAttribute;
            if (slot.isCacheableCustom() && slot.domAttribute())
                domAttribute = slot.domAttribute();

            if (kind == GetByIDKind::Try) {
                AccessCase::AccessType type;
                if (slot.isCacheableValue())
                    type = AccessCase::Load;
                else if (slot.isUnset())
                    type = AccessCase::Miss;
                else if (slot.isCacheableGetter())
                    type = AccessCase::GetGetter;
                else
                    RELEASE_ASSERT_NOT_REACHED();

                newCase = ProxyableAccessCase::create(vm, codeBlock, type, offset, structure, conditionSet, loadTargetFromProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
            } else if (!loadTargetFromProxy && getter && IntrinsicGetterAccessCase::canEmitIntrinsicGetter(getter, structure))
                newCase = IntrinsicGetterAccessCase::create(vm, codeBlock, slot.cachedOffset(), structure, conditionSet, getter, WTFMove(prototypeAccessChain));
            else {
                if (slot.isCacheableValue() || slot.isUnset()) {
                    newCase = ProxyableAccessCase::create(vm, codeBlock, slot.isUnset() ? AccessCase::Miss : AccessCase::Load,
                        offset, structure, conditionSet, loadTargetFromProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
                } else {
                    AccessCase::AccessType type;
                    if (slot.isCacheableGetter())
                        type = AccessCase::Getter;
                    else if (slot.attributes() & PropertyAttribute::CustomAccessor)
                        type = AccessCase::CustomAccessorGetter;
                    else
                        type = AccessCase::CustomValueGetter;

                    if (kind == GetByIDKind::WithThis && type == AccessCase::CustomAccessorGetter && domAttribute)
                        return GiveUpOnCache;

                    newCase = GetterSetterAccessCase::create(
                        vm, codeBlock, type, offset, structure, conditionSet, loadTargetFromProxy,
                        slot.watchpointSet(), slot.isCacheableCustom() ? slot.customGetter() : nullptr,
                        slot.isCacheableCustom() && slot.slotBase() != baseValue ? slot.slotBase() : nullptr,
                        domAttribute, WTFMove(prototypeAccessChain));
                }
            }
        }

        LOG_IC((ICEvent::GetByIdAddAccessCase, baseValue.classInfoOrNull(vm), propertyName));

        result = stubInfo.addAccessCase(locker, codeBlock, propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::GetByIdReplaceWithJump, baseValue.classInfoOrNull(vm), propertyName));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, exec->codeBlock(), result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchGetByID(ExecState* exec, JSValue baseValue, const Identifier& propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCacheGetByID(exec, baseValue, propertyName, slot, stubInfo, kind) == GiveUpOnCache) {
        CodeBlock* codeBlock = exec->codeBlock();
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateGetByIdFunction(kind));
    }
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
    VM& vm = exec->vm();
    AccessGenerationResult result;
    {
        GCSafeConcurrentJSLocker locker(exec->codeBlock()->m_lock, exec->vm().heap);

        if (forceICFailure(exec))
            return GiveUpOnCache;
        
        CodeBlock* codeBlock = exec->codeBlock();

        if (!baseValue.isCell())
            return GiveUpOnCache;
        
        if (!slot.isCacheablePut() && !slot.isCacheableCustom() && !slot.isCacheableSetter())
            return GiveUpOnCache;

        // FIXME: We should try to do something smarter here...
        if (isCopyOnWrite(structure->indexingMode()))
            return GiveUpOnCache;
        // We can't end up storing to a CoW on the prototype since it shouldn't own properties.
        ASSERT(!isCopyOnWrite(slot.base()->indexingMode()));

        if (!structure->propertyAccessesAreCacheable())
            return GiveUpOnCache;

        std::unique_ptr<AccessCase> newCase;
        JSCell* baseCell = baseValue.asCell();

        if (slot.base() == baseValue && slot.isCacheablePut()) {
            if (slot.type() == PutPropertySlot::ExistingProperty) {
                // This assert helps catch bugs if we accidentally forget to disable caching
                // when we transition then store to an existing property. This is common among
                // paths that reify lazy properties. If we reify a lazy property and forget
                // to disable caching, we may come down this path. The Replace IC does not
                // know how to model these types of structure transitions (or any structure
                // transition for that matter).
                RELEASE_ASSERT(baseValue.asCell()->structure(vm) == structure);

                structure->didCachePropertyReplacement(vm, slot.cachedOffset());
            
                if (stubInfo.cacheType == CacheType::Unset
                    && InlineAccess::canGenerateSelfPropertyReplace(stubInfo, slot.cachedOffset())
                    && !structure->needImpurePropertyWatchpoint()) {
                    
                    bool generatedCodeInline = InlineAccess::generateSelfPropertyReplace(stubInfo, structure, slot.cachedOffset());
                    if (generatedCodeInline) {
                        LOG_IC((ICEvent::PutByIdSelfPatch, structure->classInfo(), ident));
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateOptimizingPutByIdFunction(slot, putKind));
                        stubInfo.initPutByIdReplace(codeBlock, structure, slot.cachedOffset());
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::Replace, slot.cachedOffset(), structure);
            } else {
                ASSERT(slot.type() == PutPropertySlot::NewProperty);

                if (!structure->isObject())
                    return GiveUpOnCache;

                if (structure->isDictionary()) {
                    if (structure->hasBeenFlattenedBefore())
                        return GiveUpOnCache;
                    structure->flattenDictionaryStructure(vm, jsCast<JSObject*>(baseValue));
                }

                PropertyOffset offset;
                Structure* newStructure =
                    Structure::addPropertyTransitionToExistingStructureConcurrently(
                        structure, ident.impl(), 0, offset);
                if (!newStructure || !newStructure->propertyAccessesAreCacheable())
                    return GiveUpOnCache;

                ASSERT(newStructure->previousID() == structure);
                ASSERT(!newStructure->isDictionary());
                ASSERT(newStructure->isObject());
                
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
                ObjectPropertyConditionSet conditionSet;
                if (putKind == NotDirect) {
                    bool usesPolyProto;
                    prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), baseCell, nullptr, usesPolyProto);
                    if (!prototypeAccessChain) {
                        // It's invalid to access this prototype property.
                        return GiveUpOnCache;
                    }

                    if (!usesPolyProto) {
                        prototypeAccessChain = nullptr;
                        conditionSet =
                            generateConditionsForPropertySetterMiss(
                                vm, codeBlock, exec, newStructure, ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }

                }

                newCase = AccessCase::create(vm, codeBlock, offset, structure, newStructure, conditionSet, WTFMove(prototypeAccessChain));
            }
        } else if (slot.isCacheableCustom() || slot.isCacheableSetter()) {
            if (slot.isCacheableCustom()) {
                ObjectPropertyConditionSet conditionSet;
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;

                if (slot.base() != baseValue) {
                    bool usesPolyProto;
                    prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), baseCell, slot.base(), usesPolyProto);
                    if (!prototypeAccessChain) {
                        // It's invalid to access this prototype property.
                        return GiveUpOnCache;
                    }

                    if (!usesPolyProto) {
                        prototypeAccessChain = nullptr;
                        conditionSet =
                            generateConditionsForPrototypePropertyHitCustom(
                                vm, codeBlock, exec, structure, slot.base(), ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                }

                newCase = GetterSetterAccessCase::create(
                    vm, codeBlock, slot.isCustomAccessor() ? AccessCase::CustomAccessorSetter : AccessCase::CustomValueSetter, structure, invalidOffset,
                    conditionSet, WTFMove(prototypeAccessChain), slot.customSetter(), slot.base() != baseValue ? slot.base() : nullptr);
            } else {
                ObjectPropertyConditionSet conditionSet;
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
                PropertyOffset offset = slot.cachedOffset();

                if (slot.base() != baseValue) {
                    bool usesPolyProto;
                    prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), baseCell, slot.base(), usesPolyProto);
                    if (!prototypeAccessChain) {
                        // It's invalid to access this prototype property.
                        return GiveUpOnCache;
                    }

                    if (!usesPolyProto) {
                        prototypeAccessChain = nullptr;
                        conditionSet =
                            generateConditionsForPrototypePropertyHit(
                                vm, codeBlock, exec, structure, slot.base(), ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;

                        PropertyOffset conditionSetOffset = conditionSet.slotBaseCondition().offset();
                        if (UNLIKELY(offset != conditionSetOffset))
                            CRASH_WITH_INFO(offset, conditionSetOffset, slot.base()->type(), baseCell->type(), conditionSet.size());
                    }

                }

                newCase = GetterSetterAccessCase::create(
                    vm, codeBlock, AccessCase::Setter, structure, offset, conditionSet, WTFMove(prototypeAccessChain));
            }
        }

        LOG_IC((ICEvent::PutByIdAddAccessCase, structure->classInfo(), ident));
        
        result = stubInfo.addAccessCase(locker, codeBlock, ident, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByIdReplaceWithJump, structure->classInfo(), ident));
            
            RELEASE_ASSERT(result.code());

            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, exec->codeBlock(), result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchPutByID(ExecState* exec, JSValue baseValue, Structure* structure, const Identifier& propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCachePutByID(exec, baseValue, structure, propertyName, slot, stubInfo, putKind) == GiveUpOnCache) {
        CodeBlock* codeBlock = exec->codeBlock();
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateGenericPutByIdFunction(slot, putKind));
    }
}

static InlineCacheAction tryCacheInByID(
    ExecState* exec, JSObject* base, const Identifier& ident,
    bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    VM& vm = exec->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(exec->codeBlock()->m_lock, vm.heap);
        if (forceICFailure(exec))
            return GiveUpOnCache;
        
        if (!base->structure(vm)->propertyAccessesAreCacheable() || (!wasFound && !base->structure(vm)->propertyAccessesAreCacheableForAbsence()))
            return GiveUpOnCache;
        
        if (wasFound) {
            if (!slot.isCacheable())
                return GiveUpOnCache;
        }
        
        CodeBlock* codeBlock = exec->codeBlock();
        Structure* structure = base->structure(vm);
        
        std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
        ObjectPropertyConditionSet conditionSet;
        if (wasFound) {
            InlineCacheAction action = actionForCell(vm, base);
            if (action != AttemptToCache)
                return action;

            // Optimize self access.
            if (stubInfo.cacheType == CacheType::Unset
                && slot.isCacheableValue()
                && slot.slotBase() == base
                && !slot.watchpointSet()
                && !structure->needImpurePropertyWatchpoint()) {
                bool generatedCodeInline = InlineAccess::generateSelfInAccess(stubInfo, structure);
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::InByIdSelfPatch, structure->classInfo(), ident));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), operationInByIdOptimize);
                    stubInfo.initInByIdSelf(codeBlock, structure, slot.cachedOffset());
                    return RetryCacheLater;
                }
            }

            if (slot.slotBase() != base) {
                bool usesPolyProto;
                prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), base, slot, usesPolyProto);
                if (!prototypeAccessChain) {
                    // It's invalid to access this prototype property.
                    return GiveUpOnCache;
                }
                if (!usesPolyProto) {
                    prototypeAccessChain = nullptr;
                    conditionSet = generateConditionsForPrototypePropertyHit(
                        vm, codeBlock, exec, structure, slot.slotBase(), ident.impl());
                }
            }
        } else {
            bool usesPolyProto;
            prototypeAccessChain = PolyProtoAccessChain::create(exec->lexicalGlobalObject(), base, slot, usesPolyProto);
            if (!prototypeAccessChain) {
                // It's invalid to access this prototype property.
                return GiveUpOnCache;
            }

            if (!usesPolyProto) {
                prototypeAccessChain = nullptr;
                conditionSet = generateConditionsForPropertyMiss(
                    vm, codeBlock, exec, structure, ident.impl());
            }
        }
        if (!conditionSet.isValid())
            return GiveUpOnCache;

        LOG_IC((ICEvent::InAddAccessCase, structure->classInfo(), ident));

        std::unique_ptr<AccessCase> newCase = AccessCase::create(
            vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, wasFound ? slot.cachedOffset() : invalidOffset, structure, conditionSet, WTFMove(prototypeAccessChain));

        result = stubInfo.addAccessCase(locker, codeBlock, ident, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfo(), ident));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, exec->codeBlock(), result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchInByID(ExecState* exec, JSObject* baseObject, const Identifier& propertyName, bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheInByID(exec, baseObject, propertyName, wasFound, slot, stubInfo) == GiveUpOnCache) {
        CodeBlock* codeBlock = exec->codeBlock();
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), operationInById);
    }
}

static InlineCacheAction tryCacheInstanceOf(
    ExecState* exec, JSValue valueValue, JSValue prototypeValue, StructureStubInfo& stubInfo,
    bool wasFound)
{
    VM& vm = exec->vm();
    CodeBlock* codeBlock = exec->codeBlock();
    AccessGenerationResult result;
    
    RELEASE_ASSERT(valueValue.isCell()); // shouldConsiderCaching rejects non-cells.
    
    if (forceICFailure(exec))
        return GiveUpOnCache;
    
    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
        
        JSCell* value = valueValue.asCell();
        Structure* structure = value->structure(vm);
        std::unique_ptr<AccessCase> newCase;
        JSObject* prototype = jsDynamicCast<JSObject*>(vm, prototypeValue);
        if (prototype) {
            if (!jsDynamicCast<JSObject*>(vm, value)) {
                newCase = InstanceOfAccessCase::create(
                    vm, codeBlock, AccessCase::InstanceOfMiss, structure, ObjectPropertyConditionSet(),
                    prototype);
            } else if (structure->prototypeQueriesAreCacheable()) {
                // FIXME: Teach this to do poly proto.
                // https://bugs.webkit.org/show_bug.cgi?id=185663

                ObjectPropertyConditionSet conditionSet = generateConditionsForInstanceOf(
                    vm, codeBlock, exec, structure, prototype, wasFound);

                if (conditionSet.isValid()) {
                    newCase = InstanceOfAccessCase::create(
                        vm, codeBlock,
                        wasFound ? AccessCase::InstanceOfHit : AccessCase::InstanceOfMiss,
                        structure, conditionSet, prototype);
                }
            }
        }
        
        if (!newCase)
            newCase = AccessCase::create(vm, codeBlock, AccessCase::InstanceOfGeneric);
        
        LOG_IC((ICEvent::InstanceOfAddAccessCase, structure->classInfo(), Identifier()));
        
        result = stubInfo.addAccessCase(locker, codeBlock, Identifier(), WTFMove(newCase));
        
        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InstanceOfReplaceWithJump, structure->classInfo(), Identifier()));
            
            RELEASE_ASSERT(result.code());

            MacroAssembler::repatchJump(
                stubInfo.patchableJump(),
                CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }
    
    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchInstanceOf(
    ExecState* exec, JSValue valueValue, JSValue prototypeValue, StructureStubInfo& stubInfo,
    bool wasFound)
{
    SuperSamplerScope superSamplerScope(false);
    if (tryCacheInstanceOf(exec, valueValue, prototypeValue, stubInfo, wasFound) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(exec->codeBlock(), stubInfo.slowPathCallLocation(), operationInstanceOfGeneric);
}

static void linkSlowFor(VM*, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef<JITStubRoutinePtrTag> codeRef)
{
    MacroAssembler::repatchNearCall(callLinkInfo.callReturnLocation(), CodeLocationLabel<JITStubRoutinePtrTag>(codeRef.code()));
}

static void linkSlowFor(VM* vm, CallLinkInfo& callLinkInfo, ThunkGenerator generator)
{
    linkSlowFor(vm, callLinkInfo, vm->getCTIStub(generator).retagged<JITStubRoutinePtrTag>());
}

static void linkSlowFor(VM* vm, CallLinkInfo& callLinkInfo)
{
    MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(vm, callLinkInfo);
    linkSlowFor(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(createJITStubRoutine(virtualThunk, *vm, nullptr, true));
}

static JSCell* webAssemblyOwner(JSCell* callee)
{
#if ENABLE(WEBASSEMBLY)
    // Each WebAssembly.Instance shares the stubs from their WebAssembly.Module, which are therefore the appropriate owner.
    return jsCast<WebAssemblyToJSCallee*>(callee)->module();
#else
    UNUSED_PARAM(callee);
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
#endif // ENABLE(WEBASSEMBLY)
}

void linkFor(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
{
    ASSERT(!callLinkInfo.stub());

    CallFrame* callerFrame = exec->callerFrame();
    // Our caller must have a cell for a callee. When calling
    // this from Wasm, we ensure the callee is a cell.
    ASSERT(callerFrame->callee().isCell());

    VM& vm = callerFrame->vm();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();

    // WebAssembly -> JS stubs don't have a valid CodeBlock.
    JSCell* owner = isWebAssemblyToJSCallee(callerFrame->callee().asCell()) ? webAssemblyOwner(callerFrame->callee().asCell()) : callerCodeBlock;
    ASSERT(owner);

    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setCallee(vm, owner, callee);
    callLinkInfo.setLastSeenCallee(vm, owner, callee);
    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), CodeLocationLabel<JSEntryPtrTag>(codePtr));

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(callerFrame, &callLinkInfo);

    if (callLinkInfo.specializationKind() == CodeForCall && callLinkInfo.allowStubs()) {
        linkSlowFor(&vm, callLinkInfo, linkPolymorphicCallThunkGenerator);
        return;
    }
    
    linkSlowFor(&vm, callLinkInfo);
}

void linkDirectFor(
    ExecState* exec, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
{
    ASSERT(!callLinkInfo.stub());
    
    CodeBlock* callerCodeBlock = exec->codeBlock();

    VM* vm = callerCodeBlock->vm();
    
    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setCodeBlock(*vm, callerCodeBlock, jsCast<FunctionCodeBlock*>(calleeCodeBlock));
    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    if (callLinkInfo.callType() == CallLinkInfo::DirectTailCall)
        MacroAssembler::repatchJumpToNop(callLinkInfo.patchableJump());
    MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), CodeLocationLabel<JSEntryPtrTag>(codePtr));

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(exec, &callLinkInfo);
}

void linkSlowFor(
    ExecState* exec, CallLinkInfo& callLinkInfo)
{
    CodeBlock* callerCodeBlock = exec->callerFrame()->codeBlock();
    VM* vm = callerCodeBlock->vm();
    
    linkSlowFor(vm, callLinkInfo);
}

static void revertCall(VM* vm, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef<JITStubRoutinePtrTag> codeRef)
{
    if (callLinkInfo.isDirect()) {
        callLinkInfo.clearCodeBlock();
        if (callLinkInfo.callType() == CallLinkInfo::DirectTailCall)
            MacroAssembler::repatchJump(callLinkInfo.patchableJump(), callLinkInfo.slowPathStart());
        else
            MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), callLinkInfo.slowPathStart());
    } else {
        MacroAssembler::revertJumpReplacementToBranchPtrWithPatch(
            MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
            static_cast<MacroAssembler::RegisterID>(callLinkInfo.calleeGPR()), 0);
        linkSlowFor(vm, callLinkInfo, codeRef);
        callLinkInfo.clearCallee();
    }
    callLinkInfo.clearSeen();
    callLinkInfo.clearStub();
    callLinkInfo.clearSlowStub();
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void unlinkFor(VM& vm, CallLinkInfo& callLinkInfo)
{
    if (Options::dumpDisassembly())
        dataLog("Unlinking call at ", callLinkInfo.hotPathOther(), "\n");
    
    revertCall(&vm, callLinkInfo, vm.getCTIStub(linkCallThunkGenerator).retagged<JITStubRoutinePtrTag>());
}

void linkVirtualFor(ExecState* exec, CallLinkInfo& callLinkInfo)
{
    CallFrame* callerFrame = exec->callerFrame();
    VM& vm = callerFrame->vm();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();

    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking virtual call at ", FullCodeOrigin(callerCodeBlock, callerFrame->codeOrigin()), "\n");

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(&vm, callLinkInfo);
    revertCall(&vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(createJITStubRoutine(virtualThunk, vm, nullptr, true));
    callLinkInfo.setClearedByVirtual();
}

namespace {
struct CallToCodePtr {
    CCallHelpers::Call call;
    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
};
} // annonymous namespace

void linkPolymorphicCall(
    ExecState* exec, CallLinkInfo& callLinkInfo, CallVariant newVariant)
{
    RELEASE_ASSERT(callLinkInfo.allowStubs());
    
    if (!newVariant) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }

    CallFrame* callerFrame = exec->callerFrame();

    // Our caller must be have a cell for a callee. When calling
    // this from Wasm, we ensure the callee is a cell.
    ASSERT(callerFrame->callee().isCell());

    VM& vm = callerFrame->vm();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    bool isWebAssembly = isWebAssemblyToJSCallee(callerFrame->callee().asCell());

    // WebAssembly -> JS stubs don't have a valid CodeBlock.
    JSCell* owner = isWebAssembly ? webAssemblyOwner(callerFrame->callee().asCell()) : callerCodeBlock;
    ASSERT(owner);

    CallVariantList list;
    if (PolymorphicCallStubRoutine* stub = callLinkInfo.stub())
        list = stub->variants();
    else if (JSObject* oldCallee = callLinkInfo.callee())
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
        CodeBlock* codeBlock = nullptr;
        if (variant.executable() && !variant.executable()->isHostFunction()) {
            ExecutableBase* executable = variant.executable();
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
    if (isWebAssembly)
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForWebAssemblyToJS();
    else if (callerCodeBlock->jitType() == JITCode::topTierJIT())
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForTopTier();
    else
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSize();

    if (list.size() > maxPolymorphicCallVariantListSize) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }
    
    GPRReg calleeGPR = static_cast<GPRReg>(callLinkInfo.calleeGPR());
    
    CCallHelpers stubJit(callerCodeBlock);
    
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
        slowPath.append(stubJit.branchIfNotCell(calleeGPR));
#else
        // We would have already checked that the callee is a cell.
#endif

        // FIXME: We could add a fast path for InternalFunction with closure call.
        slowPath.append(stubJit.branchIfNotFunction(calleeGPR));
    
        stubJit.loadPtr(
            CCallHelpers::Address(calleeGPR, JSFunction::offsetOfExecutable()),
            scratchGPR);
        
        comparisonValueGPR = scratchGPR;
    } else
        comparisonValueGPR = calleeGPR;
    
    Vector<int64_t> caseValues(callCases.size());
    Vector<CallToCodePtr> calls(callCases.size());
    UniqueArray<uint32_t> fastCounts;
    
    if (!isWebAssembly && callerCodeBlock->jitType() != JITCode::topTierJIT())
        fastCounts = makeUniqueArray<uint32_t>(callCases.size());
    
    for (size_t i = 0; i < callCases.size(); ++i) {
        if (fastCounts)
            fastCounts[i] = 0;
        
        CallVariant variant = callCases[i].variant();
        int64_t newCaseValue = 0;
        if (isClosureCall) {
            newCaseValue = bitwise_cast<intptr_t>(variant.executable());
            // FIXME: We could add a fast path for InternalFunction with closure call.
            // https://bugs.webkit.org/show_bug.cgi?id=179311
            if (!newCaseValue)
                continue;
        } else {
            if (auto* function = variant.function())
                newCaseValue = bitwise_cast<intptr_t>(function);
            else
                newCaseValue = bitwise_cast<intptr_t>(variant.internalFunction());
        }
        
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
        
        MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
        if (variant.executable()) {
            ASSERT(variant.executable()->hasJITCodeForCall());
            codePtr = variant.executable()->generatedJITCodeForCall()->addressForCall(ArityCheckNotRequired);
        } else {
            ASSERT(variant.internalFunction());
            codePtr = vm.getCTIInternalFunctionTrampolineFor(CodeForCall);
        }
        
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
    stubJit.move(CCallHelpers::TrustedImmPtr(callLinkInfo.callReturnLocation().untaggedExecutableAddress()), GPRInfo::regT4);
    
    stubJit.restoreReturnAddressBeforeReturn(GPRInfo::regT4);
    AssemblyHelpers::Jump slow = stubJit.jump();
        
    LinkBuffer patchBuffer(stubJit, owner, JITCompilationCanFail);
    if (patchBuffer.didFailToAllocate()) {
        linkVirtualFor(exec, callLinkInfo);
        return;
    }
    
    RELEASE_ASSERT(callCases.size() == calls.size());
    for (CallToCodePtr callToCodePtr : calls) {
#if CPU(ARM_THUMB2)
        // Tail call special-casing ensures proper linking on ARM Thumb2, where a tail call jumps to an address
        // with a non-decorated bottom bit but a normal call calls an address with a decorated bottom bit.
        bool isTailCall = callToCodePtr.call.isFlagSet(CCallHelpers::Call::Tail);
        void* target = isTailCall ? callToCodePtr.codePtr.dataLocation() : callToCodePtr.codePtr.executableAddress();
        patchBuffer.link(callToCodePtr.call, FunctionPtr<JSEntryPtrTag>(MacroAssemblerCodePtr<JSEntryPtrTag>::createFromExecutableAddress(target)));
#else
        patchBuffer.link(callToCodePtr.call, FunctionPtr<JSEntryPtrTag>(callToCodePtr.codePtr));
#endif
    }
    if (isWebAssembly || JITCode::isOptimizingJIT(callerCodeBlock->jitType()))
        patchBuffer.link(done, callLinkInfo.callReturnLocation().labelAtOffset(0));
    else
        patchBuffer.link(done, callLinkInfo.hotPathOther().labelAtOffset(0));
    patchBuffer.link(slow, CodeLocationLabel<JITThunkPtrTag>(vm.getCTIStub(linkPolymorphicCallThunkGenerator).code()));
    
    auto stubRoutine = adoptRef(*new PolymorphicCallStubRoutine(
        FINALIZE_CODE_FOR(
            callerCodeBlock, patchBuffer, JITStubRoutinePtrTag,
            "Polymorphic call stub for %s, return point %p, targets %s",
                isWebAssembly ? "WebAssembly" : toCString(*callerCodeBlock).data(), callLinkInfo.callReturnLocation().labelAtOffset(0).executableAddress(),
                toCString(listDump(callCases)).data()),
        vm, owner, exec->callerFrame(), callLinkInfo, callCases,
        WTFMove(fastCounts)));
    
    MacroAssembler::replaceWithJump(
        MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
        CodeLocationLabel<JITStubRoutinePtrTag>(stubRoutine->code().code()));
    // The original slow path is unreachable on 64-bits, but still
    // reachable on 32-bits since a non-cell callee will always
    // trigger the slow path
    linkSlowFor(&vm, callLinkInfo);
    
    // If there had been a previous stub routine, that one will die as soon as the GC runs and sees
    // that it's no longer on stack.
    callLinkInfo.setStub(WTFMove(stubRoutine));
    
    // The call link info no longer has a call cache apart from the jump to the polymorphic call
    // stub.
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void resetGetByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo, GetByIDKind kind)
{
    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), appropriateOptimizingGetByIdFunction(kind));
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation());
}

void resetPutByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    V_JITOperation_ESsiJJI unoptimizedFunction = reinterpret_cast<V_JITOperation_ESsiJJI>(readPutICCallTarget(codeBlock, stubInfo.slowPathCallLocation()).executableAddress());
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

    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), optimizedFunction);
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation());
}

static void resetPatchableJump(StructureStubInfo& stubInfo)
{
    MacroAssembler::repatchJump(stubInfo.patchableJump(), stubInfo.slowPathStartLocation());
}

void resetInByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation(), operationInByIdOptimize);
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation());
}

void resetInstanceOf(StructureStubInfo& stubInfo)
{
    resetPatchableJump(stubInfo);
}

} // namespace JSC

#endif
