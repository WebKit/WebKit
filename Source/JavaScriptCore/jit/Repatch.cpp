/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include "CacheableIdentifierInlines.h"
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
#include "JSWebAssemblyModule.h"
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
#include "WebAssemblyFunction.h"
#include <wtf/CommaPrinter.h>
#include <wtf/ListDump.h>
#include <wtf/StringPrintStream.h>

namespace JSC {

static FunctionPtr<CFunctionPtrTag> readPutICCallTarget(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call)
{
    FunctionPtr<OperationPtrTag> target = MacroAssembler::readCallTarget<OperationPtrTag>(call);
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITType::FTLJIT) {
        MacroAssemblerCodePtr<JITThunkPtrTag> thunk = MacroAssemblerCodePtr<OperationPtrTag>::createFromExecutableAddress(target.executableAddress()).retagged<JITThunkPtrTag>();
        return codeBlock->vm().ftlThunks->keyForSlowPathCallThunk(thunk).callTarget().retagged<CFunctionPtrTag>();
    }
#else
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    return target.retagged<CFunctionPtrTag>();
}

void ftlThunkAwareRepatchCall(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITType::FTLJIT) {
        VM& vm = codeBlock->vm();
        FTL::Thunks& thunks = *vm.ftlThunks;
        FunctionPtr<OperationPtrTag> target = MacroAssembler::readCallTarget<OperationPtrTag>(call);
        auto slowPathThunk = MacroAssemblerCodePtr<JITThunkPtrTag>::createFromExecutableAddress(target.retaggedExecutableAddress<JITThunkPtrTag>());
        FTL::SlowPathCallKey key = thunks.keyForSlowPathCallThunk(slowPathThunk);
        key = key.withCallTarget(newCalleeFunction);
        MacroAssembler::repatchCall(call, FunctionPtr<OperationPtrTag>(thunks.getSlowPathCallThunk(vm, key).retaggedCode<OperationPtrTag>()));
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

static bool forceICFailure(JSGlobalObject*)
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

inline FunctionPtr<CFunctionPtrTag> appropriateOptimizingGetByFunction(GetByKind kind)
{
    switch (kind) {
    case GetByKind::Normal:
        return operationGetByIdOptimize;
    case GetByKind::WithThis:
        return operationGetByIdWithThisOptimize;
    case GetByKind::Try:
        return operationTryGetByIdOptimize;
    case GetByKind::Direct:
        return operationGetByIdDirectOptimize;
    case GetByKind::NormalByVal:
        return operationGetByValOptimize;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline FunctionPtr<CFunctionPtrTag> appropriateGetByFunction(GetByKind kind)
{
    switch (kind) {
    case GetByKind::Normal:
        return operationGetById;
    case GetByKind::WithThis:
        return operationGetByIdWithThis;
    case GetByKind::Try:
        return operationTryGetById;
    case GetByKind::Direct:
        return operationGetByIdDirect;
    case GetByKind::NormalByVal:
        return operationGetByValGeneric;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static InlineCacheAction tryCacheGetBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, CacheableIdentifier propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByKind kind)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm().heap);

        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        // FIXME: Cache property access for immediates.
        if (!baseValue.isCell())
            return GiveUpOnCache;
        JSCell* baseCell = baseValue.asCell();

        std::unique_ptr<AccessCase> newCase;

        if (propertyName == vm.propertyNames->length) {
            if (isJSArray(baseCell)) {
                if (stubInfo.cacheType() == CacheType::Unset
                    && slot.slotBase() == baseCell
                    && InlineAccess::isCacheableArrayLength(stubInfo, jsCast<JSArray*>(baseCell))) {

                    bool generatedCodeInline = InlineAccess::generateArrayLength(stubInfo, jsCast<JSArray*>(baseCell));
                    if (generatedCodeInline) {
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateOptimizingGetByFunction(kind));
                        stubInfo.initArrayLength();
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::ArrayLength, propertyName);
            } else if (isJSString(baseCell)) {
                if (stubInfo.cacheType() == CacheType::Unset && InlineAccess::isCacheableStringLength(stubInfo)) {
                    bool generatedCodeInline = InlineAccess::generateStringLength(stubInfo);
                    if (generatedCodeInline) {
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateOptimizingGetByFunction(kind));
                        stubInfo.initStringLength();
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::StringLength, propertyName);
            } else if (DirectArguments* arguments = jsDynamicCast<DirectArguments*>(vm, baseCell)) {
                // If there were overrides, then we can handle this as a normal property load! Guarding
                // this with such a check enables us to add an IC case for that load if needed.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::DirectArgumentsLength, propertyName);
            } else if (ScopedArguments* arguments = jsDynamicCast<ScopedArguments*>(vm, baseCell)) {
                // Ditto.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::ScopedArgumentsLength, propertyName);
            }
        }

        if (!propertyName.isSymbol() && baseCell->inherits<JSModuleNamespaceObject>(vm) && !slot.isUnset()) {
            if (auto moduleNamespaceSlot = slot.moduleNamespaceSlot())
                newCase = ModuleNamespaceAccessCase::create(vm, codeBlock, propertyName, jsCast<JSModuleNamespaceObject*>(baseCell), moduleNamespaceSlot->environment, ScopeOffset(moduleNamespaceSlot->scopeOffset));
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
            if (stubInfo.cacheType() == CacheType::Unset
                && slot.isCacheableValue()
                && slot.slotBase() == baseValue
                && !slot.watchpointSet()
                && !structure->needImpurePropertyWatchpoint()
                && !loadTargetFromProxy) {

                bool generatedCodeInline = InlineAccess::generateSelfPropertyAccess(stubInfo, structure, slot.cachedOffset());
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::GetBySelfPatch, structure->classInfo(), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateOptimizingGetByFunction(kind));
                    stubInfo.initGetByIdSelf(codeBlock, structure, slot.cachedOffset(), propertyName);
                    return RetryCacheLater;
                }
            }

            std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;

            PropertyOffset offset = slot.isUnset() ? invalidOffset : slot.cachedOffset();

            if (slot.isCustom() && slot.slotBase() == baseValue) {
                // To cache self customs, we must disallow dictionaries because we
                // need to be informed if the custom goes away since we cache the
                // constant function pointer.

                if (!prepareChainForCaching(globalObject, slot.slotBase(), slot.slotBase()))
                    return GiveUpOnCache;
            }

            if (slot.isUnset() || slot.slotBase() != baseValue) {
                if (structure->typeInfo().prohibitsPropertyCaching())
                    return GiveUpOnCache;

                if (structure->isDictionary()) {
                    if (structure->hasBeenFlattenedBefore())
                        return GiveUpOnCache;
                    structure->flattenDictionaryStructure(vm, jsCast<JSObject*>(baseCell));
                    return RetryCacheLater; // We may have changed property offsets.
                }

                if (slot.isUnset() && structure->typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence())
                    return GiveUpOnCache;

                // If a kind is GetByKind::Direct, we do not need to investigate prototype chains further.
                // Cacheability just depends on the head structure.
                if (kind != GetByKind::Direct) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->flattenedDictionary) {
                        // Property offsets may have changed due to flattening. We'll cache later.
                        return RetryCacheLater;
                    }

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::create(globalObject, baseCell, slot);
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                        RELEASE_ASSERT(slot.isCacheableCustom() || prototypeAccessChain->slotBaseStructure(vm, structure)->get(vm, propertyName.uid()) == offset);
                    } else {
                        // We use ObjectPropertyConditionSet instead for faster accesses.
                        prototypeAccessChain = nullptr;

                        // FIXME: Maybe this `if` should be inside generateConditionsForPropertyBlah.
                        // https://bugs.webkit.org/show_bug.cgi?id=185215
                        if (slot.isUnset()) {
                            conditionSet = generateConditionsForPropertyMiss(
                                vm, codeBlock, globalObject, structure, propertyName.uid());
                        } else if (!slot.isCacheableCustom()) {
                            conditionSet = generateConditionsForPrototypePropertyHit(
                                vm, codeBlock, globalObject, structure, slot.slotBase(),
                                propertyName.uid());
                            RELEASE_ASSERT(!conditionSet.isValid() || conditionSet.slotBaseCondition().offset() == offset);
                        } else {
                            conditionSet = generateConditionsForPrototypePropertyHitCustom(
                                vm, codeBlock, globalObject, structure, slot.slotBase(),
                                propertyName.uid(), slot.attributes());
                        }

                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                }
            }

            JSFunction* getter = nullptr;
            if (slot.isCacheableGetter())
                getter = jsDynamicCast<JSFunction*>(vm, slot.getterSetter()->getter());

            Optional<DOMAttributeAnnotation> domAttribute;
            if (slot.isCacheableCustom() && slot.domAttribute())
                domAttribute = slot.domAttribute();

            if (kind == GetByKind::Try) {
                AccessCase::AccessType type;
                if (slot.isCacheableValue())
                    type = AccessCase::Load;
                else if (slot.isUnset())
                    type = AccessCase::Miss;
                else if (slot.isCacheableGetter())
                    type = AccessCase::GetGetter;
                else
                    RELEASE_ASSERT_NOT_REACHED();

                newCase = ProxyableAccessCase::create(vm, codeBlock, type, propertyName, offset, structure, conditionSet, loadTargetFromProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
            } else if (!loadTargetFromProxy && getter && IntrinsicGetterAccessCase::canEmitIntrinsicGetter(getter, structure))
                newCase = IntrinsicGetterAccessCase::create(vm, codeBlock, propertyName, slot.cachedOffset(), structure, conditionSet, getter, WTFMove(prototypeAccessChain));
            else {
                if (slot.isCacheableValue() || slot.isUnset()) {
                    newCase = ProxyableAccessCase::create(vm, codeBlock, slot.isUnset() ? AccessCase::Miss : AccessCase::Load,
                        propertyName, offset, structure, conditionSet, loadTargetFromProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
                } else {
                    AccessCase::AccessType type;
                    if (slot.isCacheableGetter())
                        type = AccessCase::Getter;
                    else if (slot.attributes() & PropertyAttribute::CustomAccessor)
                        type = AccessCase::CustomAccessorGetter;
                    else
                        type = AccessCase::CustomValueGetter;

                    if (kind == GetByKind::WithThis && type == AccessCase::CustomAccessorGetter && domAttribute)
                        return GiveUpOnCache;

                    newCase = GetterSetterAccessCase::create(
                        vm, codeBlock, type, propertyName, offset, structure, conditionSet, loadTargetFromProxy,
                        slot.watchpointSet(), slot.isCacheableCustom() ? slot.customGetter() : nullptr,
                        slot.isCacheableCustom() && slot.slotBase() != baseValue ? slot.slotBase() : nullptr,
                        domAttribute, WTFMove(prototypeAccessChain));
                }
            }
        }

        LOG_IC((ICEvent::GetByAddAccessCase, baseValue.classInfoOrNull(vm), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::GetByReplaceWithJump, baseValue.classInfoOrNull(vm), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchGetBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, CacheableIdentifier propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByKind kind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCacheGetBy(globalObject, codeBlock, baseValue, propertyName, slot, stubInfo, kind) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateGetByFunction(kind));
}


static InlineCacheAction tryCacheArrayGetByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue index, StructureStubInfo& stubInfo)
{
    if (!baseValue.isCell())
        return GiveUpOnCache;

    if (!index.isInt32())
        return RetryCacheLater;

    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm().heap);

        JSCell* base = baseValue.asCell();

        AccessCase::AccessType accessType;
        if (base->type() == DirectArgumentsType)
            accessType = AccessCase::IndexedDirectArgumentsLoad;
        else if (base->type() == ScopedArgumentsType)
            accessType = AccessCase::IndexedScopedArgumentsLoad;
        else if (base->type() == StringType)
            accessType = AccessCase::IndexedStringLoad;
        else if (isTypedView(base->classInfo(vm)->typedArrayStorageType)) {
            switch (base->classInfo(vm)->typedArrayStorageType) {
            case TypeInt8:
                accessType = AccessCase::IndexedTypedArrayInt8Load;
                break;
            case TypeUint8:
                accessType = AccessCase::IndexedTypedArrayUint8Load;
                break;
            case TypeUint8Clamped:
                accessType = AccessCase::IndexedTypedArrayUint8ClampedLoad;
                break;
            case TypeInt16:
                accessType = AccessCase::IndexedTypedArrayInt16Load;
                break;
            case TypeUint16:
                accessType = AccessCase::IndexedTypedArrayUint16Load;
                break;
            case TypeInt32:
                accessType = AccessCase::IndexedTypedArrayInt32Load;
                break;
            case TypeUint32:
                accessType = AccessCase::IndexedTypedArrayUint32Load;
                break;
            case TypeFloat32:
                accessType = AccessCase::IndexedTypedArrayFloat32Load;
                break;
            case TypeFloat64:
                accessType = AccessCase::IndexedTypedArrayFloat64Load;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            IndexingType indexingShape = base->indexingType() & IndexingShapeMask;
            switch (indexingShape) {
            case Int32Shape:
                accessType = AccessCase::IndexedInt32Load;
                break;
            case DoubleShape:
                accessType = AccessCase::IndexedDoubleLoad;
                break;
            case ContiguousShape:
                accessType = AccessCase::IndexedContiguousLoad;
                break;
            case ArrayStorageShape:
                accessType = AccessCase::IndexedArrayStorageLoad;
                break;
            default:
                return GiveUpOnCache;
            }
        }

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, AccessCase::create(vm, codeBlock, accessType, nullptr));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::GetByReplaceWithJump, baseValue.classInfoOrNull(vm), Identifier()));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayGetByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo)
{
    if (tryCacheArrayGetByVal(globalObject, codeBlock, base, index, stubInfo) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationGetByValGeneric);
}

static auto appropriateGenericPutByIdFunction(const PutPropertySlot &slot, PutKind putKind) -> decltype(&operationPutByIdDirectStrict)
{
    if (slot.isStrictMode()) {
        if (putKind == Direct)
            return operationPutByIdDirectStrict;
        if (putKind == DirectPrivateFieldCreate)
            return operationPutByIdDefinePrivateFieldStrict;
        if (putKind == DirectPrivateFieldPut)
            return operationPutByIdPutPrivateFieldStrict;
        return operationPutByIdStrict;
    }
    if (putKind == Direct)
        return operationPutByIdDirectNonStrict;
    return operationPutByIdNonStrict;
}

static auto appropriateOptimizingPutByIdFunction(const PutPropertySlot &slot, PutKind putKind) -> decltype(&operationPutByIdDirectStrictOptimize)
{
    if (slot.isStrictMode()) {
        if (putKind == Direct)
            return operationPutByIdDirectStrictOptimize;
        if (putKind == DirectPrivateFieldCreate)
            return operationPutByIdDefinePrivateFieldStrictOptimize;
        if (putKind == DirectPrivateFieldPut)
            return operationPutByIdPutPrivateFieldStrictOptimize;
        return operationPutByIdStrictOptimize;
    }
    if (putKind == Direct)
        return operationPutByIdDirectNonStrictOptimize;
    return operationPutByIdNonStrictOptimize;
}

static InlineCacheAction tryCachePutByID(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, propertyName.uid());
    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm().heap);

        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        if (!baseValue.isCell())
            return GiveUpOnCache;
        
        if (!slot.isCacheablePut() && !slot.isCacheableCustom() && !slot.isCacheableSetter())
            return GiveUpOnCache;

        // FIXME: We should try to do something smarter here...
        if (isCopyOnWrite(oldStructure->indexingMode()))
            return GiveUpOnCache;
        // We can't end up storing to a CoW on the prototype since it shouldn't own properties.
        ASSERT(!isCopyOnWrite(slot.base()->indexingMode()));

        if (!oldStructure->propertyAccessesAreCacheable())
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
                RELEASE_ASSERT(baseValue.asCell()->structure(vm) == oldStructure);

                oldStructure->didCachePropertyReplacement(vm, slot.cachedOffset());
            
                if (stubInfo.cacheType() == CacheType::Unset
                    && InlineAccess::canGenerateSelfPropertyReplace(stubInfo, slot.cachedOffset())
                    && !oldStructure->needImpurePropertyWatchpoint()) {
                    
                    bool generatedCodeInline = InlineAccess::generateSelfPropertyReplace(stubInfo, oldStructure, slot.cachedOffset());
                    if (generatedCodeInline) {
                        LOG_IC((ICEvent::PutByIdSelfPatch, oldStructure->classInfo(), ident, slot.base() == baseValue));
                        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateOptimizingPutByIdFunction(slot, putKind));
                        stubInfo.initPutByIdReplace(codeBlock, oldStructure, slot.cachedOffset(), propertyName);
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::Replace, propertyName, slot.cachedOffset(), oldStructure);
            } else {
                ASSERT(slot.type() == PutPropertySlot::NewProperty);

                if (!oldStructure->isObject())
                    return GiveUpOnCache;

                // If the old structure is dictionary, it means that this is one-on-one between an object and a structure.
                // If this is NewProperty operation, generating IC for this does not offer any benefit because this transition never happens again.
                if (oldStructure->isDictionary())
                    return RetryCacheLater;

                PropertyOffset offset;
                Structure* newStructure = Structure::addPropertyTransitionToExistingStructureConcurrently(oldStructure, ident.impl(), static_cast<unsigned>(PropertyAttribute::None), offset);
                if (!newStructure || !newStructure->propertyAccessesAreCacheable())
                    return GiveUpOnCache;

                // If JSObject::put is overridden by UserObject, UserObject::put performs side-effect on JSObject::put, and it neglects to mark the PutPropertySlot as non-cachaeble,
                // then arbitrary structure transitions can happen during the put operation, and this generates wrong transition information here as if oldStructure -> newStructure.
                // In reality, the transition is oldStructure -> something unknown structures -> baseValue's structure.
                // To guard against the embedder's potentially incorrect UserObject::put implementation, we should check for this condition and if found, and give up on caching the put.
                ASSERT(baseValue.asCell()->structure(vm) == newStructure);
                if (baseValue.asCell()->structure(vm) != newStructure)
                    return GiveUpOnCache;

                ASSERT(newStructure->previousID() == oldStructure);
                ASSERT(!newStructure->isDictionary());
                ASSERT(newStructure->isObject());
                
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
                ObjectPropertyConditionSet conditionSet;
                if (putKind == NotDirect) {
                    ASSERT(putKind != DirectPrivateFieldPut && putKind != DirectPrivateFieldCreate);
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, nullptr);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::create(globalObject, baseCell, nullptr);
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                    } else {
                        prototypeAccessChain = nullptr;
                        conditionSet = generateConditionsForPropertySetterMiss(
                            vm, codeBlock, globalObject, newStructure, ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                }

                if (putKind == DirectPrivateFieldCreate) {
                    ASSERT(ident.isPrivateName());
                    conditionSet = generateConditionsForPropertyMiss(vm, codeBlock, globalObject, newStructure, ident.impl());
                    if (!conditionSet.isValid())
                        return GiveUpOnCache;
                }

                newCase = AccessCase::createTransition(vm, codeBlock, propertyName, offset, oldStructure, newStructure, conditionSet, WTFMove(prototypeAccessChain));
            }
        } else if (slot.isCacheableCustom() || slot.isCacheableSetter()) {
            if (slot.isCacheableCustom()) {
                ObjectPropertyConditionSet conditionSet;
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;

                // We need to do this even if we're a self custom, since we must disallow dictionaries
                // because we need to be informed if the custom goes away since we cache the constant
                // function pointer.
                auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot.base());
                if (!cacheStatus)
                    return GiveUpOnCache;

                if (slot.base() != baseValue) {
                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::create(globalObject, baseCell, slot.base());
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                    } else {
                        prototypeAccessChain = nullptr;
                        conditionSet = generateConditionsForPrototypePropertyHitCustom(
                            vm, codeBlock, globalObject, oldStructure, slot.base(), ident.impl(), static_cast<unsigned>(PropertyAttribute::None));
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                }

                newCase = GetterSetterAccessCase::create(
                    vm, codeBlock, slot.isCustomAccessor() ? AccessCase::CustomAccessorSetter : AccessCase::CustomValueSetter, oldStructure, propertyName,
                    invalidOffset, conditionSet, WTFMove(prototypeAccessChain), slot.customSetter(), slot.base() != baseValue ? slot.base() : nullptr);
            } else {
                ObjectPropertyConditionSet conditionSet;
                std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
                PropertyOffset offset = slot.cachedOffset();

                if (slot.base() != baseValue) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot.base());
                    if (!cacheStatus)
                        return GiveUpOnCache;
                    if (cacheStatus->flattenedDictionary)
                        return RetryCacheLater;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::create(globalObject, baseCell, slot.base());
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                        unsigned attributes;
                        offset = prototypeAccessChain->slotBaseStructure(vm, baseCell->structure(vm))->get(vm, ident.impl(), attributes);
                        if (!isValidOffset(offset) || !(attributes & PropertyAttribute::Accessor))
                            return RetryCacheLater;
                    } else {
                        prototypeAccessChain = nullptr;
                        conditionSet = generateConditionsForPrototypePropertyHit(
                            vm, codeBlock, globalObject, oldStructure, slot.base(), ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;

                        if (!(conditionSet.slotBaseCondition().attributes() & PropertyAttribute::Accessor))
                            return GiveUpOnCache;

                        offset = conditionSet.slotBaseCondition().offset();
                    }
                }

                newCase = GetterSetterAccessCase::create(
                    vm, codeBlock, AccessCase::Setter, oldStructure, propertyName, offset, conditionSet, WTFMove(prototypeAccessChain));
            }
        }

        LOG_IC((ICEvent::PutByIdAddAccessCase, oldStructure->classInfo(), ident, slot.base() == baseValue));
        
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, slot.isStrictMode() ? ECMAMode::strict() : ECMAMode::sloppy(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByIdReplaceWithJump, oldStructure->classInfo(), ident, slot.base() == baseValue));
            
            RELEASE_ASSERT(result.code());

            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchPutByID(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutKind putKind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCachePutByID(globalObject, codeBlock, baseValue, oldStructure, propertyName, slot, stubInfo, putKind) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateGenericPutByIdFunction(slot, putKind));
}

static InlineCacheAction tryCacheDeleteBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, DeletePropertySlot& slot, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, StructureStubInfo& stubInfo, DelByKind, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm().heap);

        if (forceICFailure(globalObject))
            return GiveUpOnCache;

        ASSERT(oldStructure);
        if (!baseValue.isObject() || !oldStructure->propertyAccessesAreCacheable() || oldStructure->isProxy())
            return GiveUpOnCache;

        if (!slot.isCacheableDelete())
            return GiveUpOnCache;

        if (baseValue.asCell()->structure()->isDictionary()) {
            if (baseValue.asCell()->structure()->hasBeenFlattenedBefore())
                return GiveUpOnCache;
            jsCast<JSObject*>(baseValue)->flattenDictionaryObject(vm);
            return RetryCacheLater;
        }

        if (oldStructure->isDictionary())
            return RetryCacheLater;

        std::unique_ptr<AccessCase> newCase;

        if (slot.isDeleteHit()) {
            PropertyOffset newOffset = invalidOffset;
            Structure* newStructure = Structure::removePropertyTransitionFromExistingStructureConcurrently(oldStructure, propertyName.uid(), newOffset);
            if (!newStructure)
                return RetryCacheLater;
            if (!newStructure->propertyAccessesAreCacheable() || newStructure->isDictionary())
                return GiveUpOnCache;
            ASSERT(newOffset == slot.cachedOffset());
            ASSERT(newStructure->previousID() == oldStructure);
            ASSERT(newStructure->isPropertyDeletionTransition());
            ASSERT(newStructure->isObject());
            ASSERT(isValidOffset(newOffset));
            newCase = AccessCase::createDelete(vm, codeBlock, propertyName, newOffset, oldStructure, newStructure);
        } else if (slot.isNonconfigurable()) {
            if (ecmaMode.isStrict())
                return GiveUpOnCache;

            newCase = AccessCase::create(vm, codeBlock, AccessCase::DeleteNonConfigurable, propertyName, invalidOffset, oldStructure, { }, nullptr);
        } else
            newCase = AccessCase::create(vm, codeBlock, AccessCase::DeleteMiss, propertyName, invalidOffset, oldStructure, { }, nullptr);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ecmaMode, propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            RELEASE_ASSERT(result.code());
            LOG_IC((ICEvent::DelByReplaceWithJump, oldStructure->classInfo(), Identifier::fromUid(vm, propertyName.uid())));
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchDeleteBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, DeletePropertySlot& slot, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, StructureStubInfo& stubInfo, DelByKind kind, ECMAMode ecmaMode)
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();

    if (tryCacheDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, propertyName, stubInfo, kind, ecmaMode) == GiveUpOnCache) {
        LOG_IC((ICEvent::DelByReplaceWithGeneric, baseValue.classInfoOrNull(globalObject->vm()), Identifier::fromUid(vm, propertyName.uid())));
        if (kind == DelByKind::Normal)
            ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationDeleteByIdGeneric);
        else
            ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationDeleteByValGeneric);
    }
}

static InlineCacheAction tryCacheInByID(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier propertyName,
    bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, propertyName.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        if (!base->structure(vm)->propertyAccessesAreCacheable() || (!wasFound && !base->structure(vm)->propertyAccessesAreCacheableForAbsence()))
            return GiveUpOnCache;
        
        if (wasFound) {
            if (!slot.isCacheable())
                return GiveUpOnCache;
        }
        
        Structure* structure = base->structure(vm);
        
        std::unique_ptr<PolyProtoAccessChain> prototypeAccessChain;
        ObjectPropertyConditionSet conditionSet;
        if (wasFound) {
            InlineCacheAction action = actionForCell(vm, base);
            if (action != AttemptToCache)
                return action;

            // Optimize self access.
            if (stubInfo.cacheType() == CacheType::Unset
                && slot.isCacheableValue()
                && slot.slotBase() == base
                && !slot.watchpointSet()
                && !structure->needImpurePropertyWatchpoint()) {
                bool generatedCodeInline = InlineAccess::generateSelfInAccess(stubInfo, structure);
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::InByIdSelfPatch, structure->classInfo(), ident, slot.slotBase() == base));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationInByIdOptimize);
                    stubInfo.initInByIdSelf(codeBlock, structure, slot.cachedOffset(), propertyName);
                    return RetryCacheLater;
                }
            }

            if (slot.slotBase() != base) {
                auto cacheStatus = prepareChainForCaching(globalObject, base, slot);
                if (!cacheStatus)
                    return GiveUpOnCache;
                if (cacheStatus->flattenedDictionary)
                    return RetryCacheLater;

                if (cacheStatus->usesPolyProto) {
                    prototypeAccessChain = PolyProtoAccessChain::create(globalObject, base, slot);
                    if (!prototypeAccessChain)
                        return GiveUpOnCache;
                    RELEASE_ASSERT(slot.isCacheableCustom() || prototypeAccessChain->slotBaseStructure(vm, structure)->get(vm, propertyName.uid()) == slot.cachedOffset());
                } else {
                    prototypeAccessChain = nullptr;
                    conditionSet = generateConditionsForPrototypePropertyHit(
                        vm, codeBlock, globalObject, structure, slot.slotBase(), ident.impl());
                    if (!conditionSet.isValid())
                        return GiveUpOnCache;
                    RELEASE_ASSERT(slot.isCacheableCustom() || conditionSet.slotBaseCondition().offset() == slot.cachedOffset());
                }
            }
        } else {
            auto cacheStatus = prepareChainForCaching(globalObject, base, nullptr);
            if (!cacheStatus)
                return GiveUpOnCache;

            if (cacheStatus->usesPolyProto) {
                prototypeAccessChain = PolyProtoAccessChain::create(globalObject, base, slot);
                if (!prototypeAccessChain)
                    return GiveUpOnCache;
            } else {
                prototypeAccessChain = nullptr;
                conditionSet = generateConditionsForPropertyMiss(
                    vm, codeBlock, globalObject, structure, ident.impl());
                if (!conditionSet.isValid())
                    return GiveUpOnCache;
            }
        }

        LOG_IC((ICEvent::InAddAccessCase, structure->classInfo(), ident, slot.slotBase() == base));

        std::unique_ptr<AccessCase> newCase = AccessCase::create(
            vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, propertyName, wasFound ? slot.cachedOffset() : invalidOffset, structure, conditionSet, WTFMove(prototypeAccessChain));

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfo(), ident, slot.slotBase() == base));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJump(stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchInByID(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier propertyName, bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheInByID(globalObject, codeBlock, baseObject, propertyName, wasFound, slot, stubInfo) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationInById);
}

static InlineCacheAction tryCacheInstanceOf(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue valueValue, JSValue prototypeValue, StructureStubInfo& stubInfo,
    bool wasFound)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    
    RELEASE_ASSERT(valueValue.isCell()); // shouldConsiderCaching rejects non-cells.
    
    if (forceICFailure(globalObject))
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
                prepareChainForCaching(globalObject, value, wasFound ? prototype : nullptr);
                ObjectPropertyConditionSet conditionSet = generateConditionsForInstanceOf(
                    vm, codeBlock, globalObject, structure, prototype, wasFound);

                if (conditionSet.isValid()) {
                    newCase = InstanceOfAccessCase::create(
                        vm, codeBlock,
                        wasFound ? AccessCase::InstanceOfHit : AccessCase::InstanceOfMiss,
                        structure, conditionSet, prototype);
                }
            }
        }
        
        if (!newCase)
            newCase = AccessCase::create(vm, codeBlock, AccessCase::InstanceOfGeneric, CacheableIdentifier());
        
        LOG_IC((ICEvent::InstanceOfAddAccessCase, structure->classInfo(), Identifier()));
        
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, WTFMove(newCase));
        
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
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue valueValue, JSValue prototypeValue, StructureStubInfo& stubInfo,
    bool wasFound)
{
    SuperSamplerScope superSamplerScope(false);
    if (tryCacheInstanceOf(globalObject, codeBlock, valueValue, prototypeValue, stubInfo, wasFound) == GiveUpOnCache)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationInstanceOfGeneric);
}

static void linkSlowFor(VM&, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef<JITStubRoutinePtrTag> codeRef)
{
    MacroAssembler::repatchNearCall(callLinkInfo.callReturnLocation(), CodeLocationLabel<JITStubRoutinePtrTag>(codeRef.code()));
}

static void linkSlowFor(VM& vm, CallLinkInfo& callLinkInfo, ThunkGenerator generator)
{
    linkSlowFor(vm, callLinkInfo, vm.getCTIStub(generator).retagged<JITStubRoutinePtrTag>());
}

static void linkSlowFor(VM& vm, CallLinkInfo& callLinkInfo)
{
    MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(vm, callLinkInfo);
    linkSlowFor(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(GCAwareJITStubRoutine::create(virtualThunk, vm));
}

static JSCell* webAssemblyOwner(JSCell* callee)
{
#if ENABLE(WEBASSEMBLY)
    // Each WebAssembly.Instance shares the stubs from their WebAssembly.Module, which are therefore the appropriate owner.
    return jsCast<JSWebAssemblyModule*>(callee);
#else
    UNUSED_PARAM(callee);
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
#endif // ENABLE(WEBASSEMBLY)
}

void linkFor(
    VM& vm, CallFrame* callFrame, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
{
    ASSERT(!callLinkInfo.stub());

    CallFrame* callerFrame = callFrame->callerFrame();
    // Our caller must have a cell for a callee. When calling
    // this from Wasm, we ensure the callee is a cell.
    ASSERT(callerFrame->callee().isCell());

    CodeBlock* callerCodeBlock = callerFrame->codeBlock();

    // WebAssembly -> JS stubs don't have a valid CodeBlock.
    JSCell* owner = isWebAssemblyModule(callerFrame->callee().asCell()) ? webAssemblyOwner(callerFrame->callee().asCell()) : callerCodeBlock;
    ASSERT(owner);

    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setCallee(vm, owner, callee);
    MacroAssembler::repatchPointer(callLinkInfo.hotPathBegin(), callee);
    callLinkInfo.setLastSeenCallee(vm, owner, callee);
    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), CodeLocationLabel<JSEntryPtrTag>(codePtr));

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(callerFrame, &callLinkInfo);

    if (callLinkInfo.specializationKind() == CodeForCall && callLinkInfo.allowStubs()) {
        linkSlowFor(vm, callLinkInfo, linkPolymorphicCallThunkGenerator);
        return;
    }
    
    linkSlowFor(vm, callLinkInfo);
}

void linkDirectFor(
    CallFrame* callFrame, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock,
    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
{
    ASSERT(!callLinkInfo.stub());
    
    CodeBlock* callerCodeBlock = callFrame->codeBlock();

    VM& vm = callerCodeBlock->vm();
    
    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setCodeBlock(vm, callerCodeBlock, jsCast<FunctionCodeBlock*>(calleeCodeBlock));
    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    if (callLinkInfo.callType() == CallLinkInfo::DirectTailCall)
        MacroAssembler::repatchJumpToNop(callLinkInfo.patchableJump());
    MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), CodeLocationLabel<JSEntryPtrTag>(codePtr));

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(callFrame, &callLinkInfo);
}

void linkSlowFor(CallFrame* callFrame, CallLinkInfo& callLinkInfo)
{
    CodeBlock* callerCodeBlock = callFrame->callerFrame()->codeBlock();
    VM& vm = callerCodeBlock->vm();
    
    linkSlowFor(vm, callLinkInfo);
}

static void revertCall(VM& vm, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef<JITStubRoutinePtrTag> codeRef)
{
    if (callLinkInfo.isDirect()) {
        callLinkInfo.clearCodeBlock();
        if (!callLinkInfo.clearedByJettison()) {
            if (callLinkInfo.callType() == CallLinkInfo::DirectTailCall)
                MacroAssembler::repatchJump(callLinkInfo.patchableJump(), callLinkInfo.slowPathStart());
            else
                MacroAssembler::repatchNearCall(callLinkInfo.hotPathOther(), callLinkInfo.slowPathStart());
        }
    } else {
        if (!callLinkInfo.clearedByJettison()) {
            MacroAssembler::revertJumpReplacementToBranchPtrWithPatch(
                MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
                callLinkInfo.calleeGPR(), nullptr);
            linkSlowFor(vm, callLinkInfo, codeRef);
            MacroAssembler::repatchPointer(callLinkInfo.hotPathBegin(), nullptr);
        }
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
    dataLogLnIf(Options::dumpDisassembly(), "Unlinking call at ", callLinkInfo.hotPathOther());
    
    revertCall(vm, callLinkInfo, vm.getCTIStub(linkCallThunkGenerator).retagged<JITStubRoutinePtrTag>());
}

static void linkVirtualFor(VM& vm, CallFrame* callFrame, CallLinkInfo& callLinkInfo)
{
    CallFrame* callerFrame = callFrame->callerFrame();
    CodeBlock* callerCodeBlock = callerFrame->codeBlock();

    dataLogLnIf(shouldDumpDisassemblyFor(callerCodeBlock),
        "Linking virtual call at ", FullCodeOrigin(callerCodeBlock, callerFrame->codeOrigin()));

    MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(vm, callLinkInfo);
    revertCall(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(GCAwareJITStubRoutine::create(virtualThunk, vm));
    callLinkInfo.setClearedByVirtual();
}

namespace {
struct CallToCodePtr {
    CCallHelpers::Call call;
    MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
};
} // annonymous namespace

void linkPolymorphicCall(JSGlobalObject* globalObject, CallFrame* callFrame, CallLinkInfo& callLinkInfo, CallVariant newVariant)
{
    RELEASE_ASSERT(callLinkInfo.allowStubs());

    CallFrame* callerFrame = callFrame->callerFrame();
    VM& vm = globalObject->vm();

    // During execution of linkPolymorphicCall, we strongly assume that we never do GC.
    // GC jettisons CodeBlocks, changes CallLinkInfo etc. and breaks assumption done before and after this call.
    DeferGCForAWhile deferGCForAWhile(vm.heap);
    
    if (!newVariant) {
        linkVirtualFor(vm, callFrame, callLinkInfo);
        return;
    }

    // Our caller must be have a cell for a callee. When calling
    // this from Wasm, we ensure the callee is a cell.
    ASSERT(callerFrame->callee().isCell());

    CodeBlock* callerCodeBlock = callerFrame->codeBlock();
    bool isWebAssembly = isWebAssemblyModule(callerFrame->callee().asCell());

    // WebAssembly -> JS stubs don't have a valid CodeBlock.
    JSCell* owner = isWebAssembly ? webAssemblyOwner(callerFrame->callee().asCell()) : callerCodeBlock;
    ASSERT(owner);

    CallVariantList list;
    if (PolymorphicCallStubRoutine* stub = callLinkInfo.stub())
        list = stub->variants();
    else if (JSObject* oldCallee = callLinkInfo.callee())
        list = CallVariantList { CallVariant(oldCallee) };
    
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
    Vector<int64_t> caseValues;
    
    // Figure out what our cases are.
    for (CallVariant variant : list) {
        CodeBlock* codeBlock = nullptr;
        if (variant.executable() && !variant.executable()->isHostFunction()) {
            ExecutableBase* executable = variant.executable();
            codeBlock = jsCast<FunctionExecutable*>(executable)->codeBlockForCall();
            // If we cannot handle a callee, either because we don't have a CodeBlock or because arity mismatch,
            // assume that it's better for this whole thing to be a virtual call.
            if (!codeBlock || callFrame->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo.isVarargs()) {
                linkVirtualFor(vm, callFrame, callLinkInfo);
                return;
            }
        }

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

        if (ASSERT_ENABLED) {
            if (caseValues.contains(newCaseValue)) {
                dataLog("ERROR: Attempt to add duplicate case value.\n");
                dataLog("Existing case values: ");
                CommaPrinter comma;
                for (auto& value : caseValues)
                    dataLog(comma, value);
                dataLog("\n");
                dataLog("Attempting to add: ", newCaseValue, "\n");
                dataLog("Variant list: ", listDump(callCases), "\n");
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        callCases.append(PolymorphicCallCase(variant, codeBlock));
        caseValues.append(newCaseValue);
    }
    ASSERT(callCases.size() == caseValues.size());

    // If we are over the limit, just use a normal virtual call.
    unsigned maxPolymorphicCallVariantListSize;
    if (isWebAssembly)
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForWebAssemblyToJS();
    else if (callerCodeBlock->jitType() == JITCode::topTierJIT())
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForTopTier();
    else
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSize();

    // We use list.size() instead of callCases.size() because we respect CallVariant size for now.
    if (list.size() > maxPolymorphicCallVariantListSize) {
        linkVirtualFor(vm, callFrame, callLinkInfo);
        return;
    }

    Vector<CallToCodePtr> calls(callCases.size());
    UniqueArray<uint32_t> fastCounts;

    if (!isWebAssembly && callerCodeBlock->jitType() != JITCode::topTierJIT()) {
        fastCounts = makeUniqueArray<uint32_t>(callCases.size());
        memset(fastCounts.get(), 0, callCases.size() * sizeof(uint32_t));
    }
    
    GPRReg calleeGPR = callLinkInfo.calleeGPR();

    CCallHelpers stubJit(callerCodeBlock);

    std::unique_ptr<CallFrameShuffler> frameShuffler;
    if (callLinkInfo.frameShuffleData()) {
        ASSERT(callLinkInfo.isTailCall());
        frameShuffler = makeUnique<CallFrameShuffler>(stubJit, *callLinkInfo.frameShuffleData());
#if USE(JSVALUE32_64)
        // We would have already checked that the callee is a cell, and we can
        // use the additional register this buys us.
        frameShuffler->assumeCalleeIsCell();
#endif
        frameShuffler->lockGPR(calleeGPR);
    }

    GPRReg comparisonValueGPR;
    if (isClosureCall) {
        if (frameShuffler)
            comparisonValueGPR = frameShuffler->acquireGPR();
        else
            comparisonValueGPR = AssemblyHelpers::selectScratchGPR(calleeGPR);
    } else
        comparisonValueGPR = calleeGPR;

    GPRReg fastCountsBaseGPR;
    if (frameShuffler)
        fastCountsBaseGPR = frameShuffler->acquireGPR();
    else {
        fastCountsBaseGPR =
            AssemblyHelpers::selectScratchGPR(calleeGPR, comparisonValueGPR, GPRInfo::regT3);
    }
    stubJit.move(CCallHelpers::TrustedImmPtr(fastCounts.get()), fastCountsBaseGPR);

    if (!frameShuffler && callLinkInfo.isTailCall()) {
        // We strongly assume that calleeGPR is not a callee save register in the slow path.
        ASSERT(!callerCodeBlock->calleeSaveRegisters()->find(calleeGPR));
        stubJit.emitRestoreCalleeSaves();
    }

    CCallHelpers::JumpList slowPath;
    if (isClosureCall) {
        // Verify that we have a function and stash the executable in scratchGPR.
#if USE(JSVALUE64)
        if (callLinkInfo.isTailCall())
            slowPath.append(stubJit.branchIfNotCell(calleeGPR, DoNotHaveTagRegisters));
        else
            slowPath.append(stubJit.branchIfNotCell(calleeGPR));
#else
        // We would have already checked that the callee is a cell.
#endif
        // FIXME: We could add a fast path for InternalFunction with closure call.
        slowPath.append(stubJit.branchIfNotFunction(calleeGPR));

        stubJit.loadPtr(CCallHelpers::Address(calleeGPR, JSFunction::offsetOfExecutableOrRareData()), comparisonValueGPR);
        auto hasExecutable = stubJit.branchTestPtr(CCallHelpers::Zero, comparisonValueGPR, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
        stubJit.loadPtr(CCallHelpers::Address(comparisonValueGPR, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), comparisonValueGPR);
        hasExecutable.link(&stubJit);
    }

    BinarySwitch binarySwitch(comparisonValueGPR, caseValues, BinarySwitch::IntPtr);
    CCallHelpers::JumpList done;
    while (binarySwitch.advance(stubJit)) {
        size_t caseIndex = binarySwitch.caseIndex();
        
        CallVariant variant = callCases[caseIndex].variant();
        
        MacroAssemblerCodePtr<JSEntryPtrTag> codePtr;
        if (variant.executable()) {
            ASSERT(variant.executable()->hasJITCodeForCall());
            
            codePtr = jsToWasmICCodePtr(vm, callLinkInfo.specializationKind(), variant.function());
            if (!codePtr)
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
    stubJit.move(CCallHelpers::TrustedImmPtr(globalObject), GPRInfo::regT3);
    stubJit.move(CCallHelpers::TrustedImmPtr(&callLinkInfo), GPRInfo::regT2);
    stubJit.move(CCallHelpers::TrustedImmPtr(callLinkInfo.callReturnLocation().untaggedExecutableAddress()), GPRInfo::regT4);
    
    stubJit.restoreReturnAddressBeforeReturn(GPRInfo::regT4);
    AssemblyHelpers::Jump slow = stubJit.jump();
        
    LinkBuffer patchBuffer(stubJit, owner, JITCompilationCanFail);
    if (patchBuffer.didFailToAllocate()) {
        linkVirtualFor(vm, callFrame, callLinkInfo);
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
        vm, owner, callFrame->callerFrame(), callLinkInfo, callCases,
        WTFMove(fastCounts)));
    
    MacroAssembler::replaceWithJump(
        MacroAssembler::startOfBranchPtrWithPatchOnRegister(callLinkInfo.hotPathBegin()),
        CodeLocationLabel<JITStubRoutinePtrTag>(stubRoutine->code().code()));
    // The original slow path is unreachable on 64-bits, but still
    // reachable on 32-bits since a non-cell callee will always
    // trigger the slow path
    linkSlowFor(vm, callLinkInfo);
    
    // If there had been a previous stub routine, that one will die as soon as the GC runs and sees
    // that it's no longer on stack.
    callLinkInfo.setStub(WTFMove(stubRoutine));
    
    // The call link info no longer has a call cache apart from the jump to the polymorphic call
    // stub.
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void resetGetBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, GetByKind kind)
{
    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, appropriateOptimizingGetByFunction(kind));
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation);
}

void resetPutByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    using FunctionType = decltype(&operationPutByIdDirectStrictOptimize);
    FunctionType unoptimizedFunction = reinterpret_cast<FunctionType>(readPutICCallTarget(codeBlock, stubInfo.slowPathCallLocation).executableAddress());
    FunctionType optimizedFunction;
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

    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, optimizedFunction);
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation);
}

void resetDelBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, DelByKind kind)
{
    if (kind == DelByKind::Normal)
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationDeleteByIdOptimize);
    else
        ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationDeleteByValOptimize);
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation);
}

static void resetPatchableJump(StructureStubInfo& stubInfo)
{
    MacroAssembler::repatchJump(stubInfo.patchableJump(), stubInfo.slowPathStartLocation);
}

void resetInByID(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    ftlThunkAwareRepatchCall(codeBlock, stubInfo.slowPathCallLocation, operationInByIdOptimize);
    InlineAccess::rewireStubAsJump(stubInfo, stubInfo.slowPathStartLocation);
}

void resetInstanceOf(StructureStubInfo& stubInfo)
{
    resetPatchableJump(stubInfo);
}

MacroAssemblerCodePtr<JSEntryPtrTag> jsToWasmICCodePtr(VM& vm, CodeSpecializationKind kind, JSObject* callee)
{
#if ENABLE(WEBASSEMBLY)
    if (!callee)
        return nullptr;
    if (kind != CodeForCall)
        return nullptr;
    if (auto* wasmFunction = jsDynamicCast<WebAssemblyFunction*>(vm, callee))
        return wasmFunction->jsCallEntrypoint();
#else
    UNUSED_PARAM(vm);
    UNUSED_PARAM(kind);
    UNUSED_PARAM(callee);
#endif
    return nullptr;
}

} // namespace JSC

#endif
