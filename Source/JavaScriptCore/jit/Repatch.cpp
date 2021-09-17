/*
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

static FunctionPtr<CFunctionPtrTag> readPutICCallTarget(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    if (codeBlock->useDataIC())
        return stubInfo.m_slowOperation.retagged<CFunctionPtrTag>();
    CodeLocationCall<JSInternalPtrTag> call = stubInfo.m_slowPathCallLocation;
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITType::FTLJIT) {
        FunctionPtr<JITThunkPtrTag> target = MacroAssembler::readCallTarget<JITThunkPtrTag>(call);
        MacroAssemblerCodePtr<JITThunkPtrTag> thunk = MacroAssemblerCodePtr<JITThunkPtrTag>::createFromExecutableAddress(target.executableAddress());
        return codeBlock->vm().ftlThunks->keyForSlowPathCallThunk(thunk).callTarget().retagged<CFunctionPtrTag>();
    }
#else
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    FunctionPtr<OperationPtrTag> target = MacroAssembler::readCallTarget<OperationPtrTag>(call);
    return target.retagged<CFunctionPtrTag>();
}

void ftlThunkAwareRepatchCall(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITType::FTLJIT) {
        VM& vm = codeBlock->vm();
        FTL::Thunks& thunks = *vm.ftlThunks;
        FunctionPtr<JITThunkPtrTag> target = MacroAssembler::readCallTarget<JITThunkPtrTag>(call);
        auto slowPathThunk = MacroAssemblerCodePtr<JITThunkPtrTag>::createFromExecutableAddress(target.executableAddress());
        FTL::SlowPathCallKey key = thunks.keyForSlowPathCallThunk(slowPathThunk);
        key = key.withCallTarget(newCalleeFunction);
        MacroAssembler::repatchCall(call, FunctionPtr<JITThunkPtrTag>(thunks.getSlowPathCallThunk(vm, key).code()));
        return;
    }
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    MacroAssembler::repatchCall(call, newCalleeFunction.retagged<OperationPtrTag>());
}

static void repatchSlowPathCall(CodeBlock* codeBlock, StructureStubInfo& stubInfo, FunctionPtr<CFunctionPtrTag> newCalleeFunction)
{
    if (codeBlock->useDataIC()) {
        stubInfo.m_slowOperation = newCalleeFunction.retagged<OperationPtrTag>();
        return;
    }
    ftlThunkAwareRepatchCall(codeBlock, stubInfo.m_slowPathCallLocation, newCalleeFunction);
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

        {
            GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
            stubInfo.reset(locker, codeBlock);
        }
    }
}

inline FunctionPtr<CFunctionPtrTag> appropriateOptimizingGetByFunction(GetByKind kind)
{
    switch (kind) {
    case GetByKind::ById:
        return operationGetByIdOptimize;
    case GetByKind::ByIdWithThis:
        return operationGetByIdWithThisOptimize;
    case GetByKind::TryById:
        return operationTryGetByIdOptimize;
    case GetByKind::ByIdDirect:
        return operationGetByIdDirectOptimize;
    case GetByKind::ByVal:
        return operationGetByValOptimize;
    case GetByKind::PrivateName:
        return operationGetPrivateNameOptimize;
    case GetByKind::PrivateNameById:
        return operationGetPrivateNameByIdOptimize;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline FunctionPtr<CFunctionPtrTag> appropriateGetByFunction(GetByKind kind)
{
    switch (kind) {
    case GetByKind::ById:
        return operationGetById;
    case GetByKind::ByIdWithThis:
        return operationGetByIdWithThis;
    case GetByKind::TryById:
        return operationTryGetById;
    case GetByKind::ByIdDirect:
        return operationGetByIdDirect;
    case GetByKind::ByVal:
        return operationGetByValGeneric;
    case GetByKind::PrivateName:
        return operationGetPrivateName;
    case GetByKind::PrivateNameById:
        return operationGetPrivateNameById;
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
        const bool isPrivate = kind == GetByKind::PrivateName || kind == GetByKind::PrivateNameById;

        RefPtr<AccessCase> newCase;

        if (propertyName == vm.propertyNames->length) {
            if (isJSArray(baseCell)) {
                if (stubInfo.cacheType() == CacheType::Unset
                    && slot.slotBase() == baseCell
                    && InlineAccess::isCacheableArrayLength(stubInfo, jsCast<JSArray*>(baseCell))) {

                    bool generatedCodeInline = InlineAccess::generateArrayLength(stubInfo, jsCast<JSArray*>(baseCell));
                    if (generatedCodeInline) {
                        repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingGetByFunction(kind));
                        stubInfo.initArrayLength(locker);
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::ArrayLength, propertyName);
            } else if (isJSString(baseCell)) {
                if (stubInfo.cacheType() == CacheType::Unset
                    && InlineAccess::isCacheableStringLength(stubInfo)) {
                    bool generatedCodeInline = InlineAccess::generateStringLength(stubInfo);
                    if (generatedCodeInline) {
                        repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingGetByFunction(kind));
                        stubInfo.initStringLength(locker);
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
                if (isPrivate)
                    return GiveUpOnCache;
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
                    repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingGetByFunction(kind));
                    stubInfo.initGetByIdSelf(locker, codeBlock, structure, slot.cachedOffset(), propertyName);
                    return RetryCacheLater;
                }
            }

            RefPtr<PolyProtoAccessChain> prototypeAccessChain;

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

                // If a kind is GetByKind::ByIdDirect or GetByKind::PrivateName, we do not need to investigate prototype chains further.
                // Cacheability just depends on the head structure.
                if (kind != GetByKind::ByIdDirect && !isPrivate) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->flattenedDictionary) {
                        // Property offsets may have changed due to flattening. We'll cache later.
                        return RetryCacheLater;
                    }

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, slot);
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

            std::optional<DOMAttributeAnnotation> domAttribute;
            if (slot.isCacheableCustom() && slot.domAttribute())
                domAttribute = slot.domAttribute();

            if (kind == GetByKind::TryById) {
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
                if (isPrivate) {
                    RELEASE_ASSERT(!slot.isUnset());
                    constexpr bool isProxy = false;
                    if (!slot.isCacheable())
                        return GiveUpOnCache;
                    newCase = ProxyableAccessCase::create(vm, codeBlock, AccessCase::Load, propertyName, offset, structure,
                        conditionSet, isProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
                } else if (slot.isCacheableValue() || slot.isUnset()) {
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

                    if (kind == GetByKind::ByIdWithThis && type == AccessCase::CustomAccessorGetter && domAttribute)
                        return GiveUpOnCache;

                    newCase = GetterSetterAccessCase::create(
                        vm, codeBlock, type, propertyName, offset, structure, conditionSet, loadTargetFromProxy,
                        slot.watchpointSet(), slot.isCacheableCustom() ? FunctionPtr<CustomAccessorPtrTag>(slot.customGetter()) : nullptr,
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
            switch (kind) {
            case GetByKind::ById:
            case GetByKind::ByIdWithThis:
            case GetByKind::TryById:
            case GetByKind::ByIdDirect:
            case GetByKind::PrivateNameById:
                InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
                break;
            case GetByKind::ByVal:
            case GetByKind::PrivateName:
                InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
                break;
            }

        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchGetBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, CacheableIdentifier propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByKind kind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCacheGetBy(globalObject, codeBlock, baseValue, propertyName, slot, stubInfo, kind) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByFunction(kind));
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
            // FIXME: Optimize BigInt64Array / BigUint64Array in IC
            // https://bugs.webkit.org/show_bug.cgi?id=221183
            case TypeBigInt64:
            case TypeBigUint64:
                return GiveUpOnCache;
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
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayGetByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo)
{
    if (tryCacheArrayGetByVal(globalObject, codeBlock, base, index, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationGetByValGeneric);
}

static FunctionPtr<CFunctionPtrTag> appropriateGenericPutByFunction(const PutPropertySlot &slot, PutByKind putByKind, PutKind putKind)
{
    switch (putByKind) {
    case PutByKind::ById: {
        switch (putKind) {
        case PutKind::NotDirect:
            if (slot.isStrictMode())
                return operationPutByIdStrict;
            return operationPutByIdNonStrict;
        case PutKind::Direct:
            if (slot.isStrictMode())
                return operationPutByIdDirectStrict;
            return operationPutByIdDirectNonStrict;
        case PutKind::DirectPrivateFieldDefine:
            ASSERT(slot.isStrictMode());
            return operationPutByIdDefinePrivateFieldStrict;
        case PutKind::DirectPrivateFieldSet:
            ASSERT(slot.isStrictMode());
            return operationPutByIdSetPrivateFieldStrict;
        }
        break;
    }
    case PutByKind::ByVal: {
        switch (putKind) {
        case PutKind::NotDirect:
            if (slot.isStrictMode())
                return operationPutByValStrictGeneric;
            return operationPutByValNonStrictGeneric;
        case PutKind::Direct:
            if (slot.isStrictMode())
                return operationDirectPutByValStrictGeneric;
            return operationDirectPutByValNonStrictGeneric;
        case PutKind::DirectPrivateFieldDefine:
            ASSERT(slot.isStrictMode());
            return operationPutByValDefinePrivateFieldGeneric;
        case PutKind::DirectPrivateFieldSet:
            ASSERT(slot.isStrictMode());
            return operationPutByValSetPrivateFieldGeneric;
        }
        break;
    }
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

static FunctionPtr<CFunctionPtrTag> appropriateOptimizingPutByFunction(const PutPropertySlot &slot, PutByKind putByKind, PutKind putKind)
{
    switch (putByKind) {
    case PutByKind::ById:
        switch (putKind) {
        case PutKind::NotDirect:
            if (slot.isStrictMode())
                return operationPutByIdStrictOptimize;
            return operationPutByIdNonStrictOptimize;
        case PutKind::Direct:
            if (slot.isStrictMode())
                return operationPutByIdDirectStrictOptimize;
            return operationPutByIdDirectNonStrictOptimize;
        case PutKind::DirectPrivateFieldDefine:
            ASSERT(slot.isStrictMode());
            return operationPutByIdDefinePrivateFieldStrictOptimize;
        case PutKind::DirectPrivateFieldSet:
            ASSERT(slot.isStrictMode());
            return operationPutByIdSetPrivateFieldStrictOptimize;
        }
        break;
    case PutByKind::ByVal:
        switch (putKind) {
        case PutKind::NotDirect:
            if (slot.isStrictMode())
                return operationPutByValStrictOptimize;
            return operationPutByValNonStrictOptimize;
        case PutKind::Direct:
            if (slot.isStrictMode())
                return operationDirectPutByValStrictOptimize;
            return operationDirectPutByValNonStrictOptimize;
        case PutKind::DirectPrivateFieldDefine:
            ASSERT(slot.isStrictMode());
            return operationPutByValDefinePrivateFieldOptimize;
        case PutKind::DirectPrivateFieldSet:
            ASSERT(slot.isStrictMode());
            return operationPutByValSetPrivateFieldOptimize;
        }
        break;
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

static InlineCacheAction tryCachePutBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutByKind putByKind, PutKind putKind)
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

        JSCell* baseCell = baseValue.asCell();

        bool isProxy = false;
        if (baseCell->type() == PureForwardingProxyType) {
            baseCell = jsCast<JSProxy*>(baseCell)->target();
            baseValue = baseCell;
            isProxy = true;

            // We currently only cache Replace and JS/Custom Setters on JSProxy. We don't
            // cache transitions because global objects will never share the same structure
            // in our current implementation.
            bool isCacheableProxy = (slot.isCacheablePut() && slot.type() == PutPropertySlot::ExistingProperty)
                || slot.isCacheableSetter()
                || slot.isCacheableCustom();
            if (!isCacheableProxy)
                return GiveUpOnCache;
        }

        if (isProxy && (putKind == PutKind::DirectPrivateFieldSet || putKind == PutKind::DirectPrivateFieldDefine))
            return GiveUpOnCache;

        RefPtr<AccessCase> newCase;

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
                    && !oldStructure->needImpurePropertyWatchpoint()
                    && !isProxy) {
                    
                    bool generatedCodeInline = InlineAccess::generateSelfPropertyReplace(stubInfo, oldStructure, slot.cachedOffset());
                    if (generatedCodeInline) {
                        LOG_IC((ICEvent::PutBySelfPatch, oldStructure->classInfo(), ident, slot.base() == baseValue));
                        repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingPutByFunction(slot, putByKind, putKind));
                        stubInfo.initPutByIdReplace(locker, codeBlock, oldStructure, slot.cachedOffset(), propertyName);
                        return RetryCacheLater;
                    }
                }

                newCase = ProxyableAccessCase::create(vm, codeBlock, AccessCase::Replace, propertyName, slot.cachedOffset(), oldStructure, ObjectPropertyConditionSet(), isProxy);
            } else {
                ASSERT(!isProxy);
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
                
                RefPtr<PolyProtoAccessChain> prototypeAccessChain;
                ObjectPropertyConditionSet conditionSet;
                if (putKind == PutKind::NotDirect) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, nullptr);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, nullptr);
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

                if (putKind == PutKind::DirectPrivateFieldDefine) {
                    ASSERT(ident.isPrivateName());
                    conditionSet = generateConditionsForPropertyMiss(vm, codeBlock, globalObject, newStructure, ident.impl());
                    if (!conditionSet.isValid())
                        return GiveUpOnCache;
                }

                newCase = AccessCase::createTransition(vm, codeBlock, propertyName, offset, oldStructure, newStructure, conditionSet, WTFMove(prototypeAccessChain), stubInfo);
            }
        } else if (slot.isCacheableCustom() || slot.isCacheableSetter()) {
            if (slot.isCacheableCustom()) {
                ObjectPropertyConditionSet conditionSet;
                RefPtr<PolyProtoAccessChain> prototypeAccessChain;

                // We need to do this even if we're a self custom, since we must disallow dictionaries
                // because we need to be informed if the custom goes away since we cache the constant
                // function pointer.
                auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot.base());
                if (!cacheStatus)
                    return GiveUpOnCache;

                if (slot.base() != baseValue) {
                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, slot.base());
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
                    invalidOffset, conditionSet, WTFMove(prototypeAccessChain), isProxy, slot.customSetter().retagged<CustomAccessorPtrTag>(), slot.base() != baseValue ? slot.base() : nullptr);
            } else {
                ASSERT(slot.isCacheableSetter());
                ObjectPropertyConditionSet conditionSet;
                RefPtr<PolyProtoAccessChain> prototypeAccessChain;
                PropertyOffset offset = slot.cachedOffset();

                if (slot.base() != baseValue) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, slot.base());
                    if (!cacheStatus)
                        return GiveUpOnCache;
                    if (cacheStatus->flattenedDictionary)
                        return RetryCacheLater;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, slot.base());
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
                    vm, codeBlock, AccessCase::Setter, oldStructure, propertyName, offset, conditionSet, WTFMove(prototypeAccessChain), isProxy);
            }
        }

        LOG_IC((ICEvent::PutByAddAccessCase, oldStructure->classInfo(), ident, slot.base() == baseValue));
        
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, slot.isStrictMode() ? ECMAMode::strict() : ECMAMode::sloppy(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByReplaceWithJump, oldStructure->classInfo(), ident, slot.base() == baseValue));
            
            RELEASE_ASSERT(result.code());

            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchPutBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutByKind putByKind, PutKind putKind)
{
    SuperSamplerScope superSamplerScope(false);
    
    if (tryCachePutBy(globalObject, codeBlock, baseValue, oldStructure, propertyName, slot, stubInfo, putByKind, putKind) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, appropriateGenericPutByFunction(slot, putByKind, putKind));
}

static InlineCacheAction tryCacheArrayPutByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue index, StructureStubInfo& stubInfo, PutKind)
{
    if (!baseValue.isCell())
        return GiveUpOnCache;

    if (!index.isInt32())
        return RetryCacheLater;

    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        JSCell* base = baseValue.asCell();

        AccessCase::AccessType accessType;
        if (isTypedView(base->classInfo(vm)->typedArrayStorageType)) {
            switch (base->classInfo(vm)->typedArrayStorageType) {
            case TypeInt8:
                accessType = AccessCase::IndexedTypedArrayInt8Store;
                break;
            case TypeUint8:
                accessType = AccessCase::IndexedTypedArrayUint8Store;
                break;
            case TypeUint8Clamped:
                accessType = AccessCase::IndexedTypedArrayUint8ClampedStore;
                break;
            case TypeInt16:
                accessType = AccessCase::IndexedTypedArrayInt16Store;
                break;
            case TypeUint16:
                accessType = AccessCase::IndexedTypedArrayUint16Store;
                break;
            case TypeInt32:
                accessType = AccessCase::IndexedTypedArrayInt32Store;
                break;
            case TypeUint32:
                accessType = AccessCase::IndexedTypedArrayUint32Store;
                break;
            case TypeFloat32:
                accessType = AccessCase::IndexedTypedArrayFloat32Store;
                break;
            case TypeFloat64:
                accessType = AccessCase::IndexedTypedArrayFloat64Store;
                break;
            // FIXME: Optimize BigInt64Array / BigUint64Array in IC
            // https://bugs.webkit.org/show_bug.cgi?id=221183
            case TypeBigInt64:
            case TypeBigUint64:
                return GiveUpOnCache;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            IndexingType indexingShape = base->indexingType() & IndexingShapeMask;
            switch (indexingShape) {
            case Int32Shape:
                accessType = AccessCase::IndexedInt32Store;
                break;
            case DoubleShape:
                accessType = AccessCase::IndexedDoubleStore;
                break;
            case ContiguousShape:
                accessType = AccessCase::IndexedContiguousStore;
                break;
            case ArrayStorageShape:
                accessType = AccessCase::IndexedArrayStorageStore;
                break;
            default:
                return GiveUpOnCache;
            }
        }

        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm().heap);
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, AccessCase::create(vm, codeBlock, accessType, nullptr));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByReplaceWithJump, baseValue.classInfoOrNull(vm), Identifier()));

            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayPutByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo, PutKind putKind, ECMAMode ecmaMode)
{
    if (tryCacheArrayPutByVal(globalObject, codeBlock, base, index, stubInfo, putKind) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, putKind == PutKind::Direct ? (ecmaMode.isStrict() ? operationDirectPutByValStrictGeneric : operationDirectPutByValNonStrictGeneric) : (ecmaMode.isStrict() ? operationPutByValStrictGeneric : operationPutByValNonStrictGeneric));
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

        RefPtr<AccessCase> newCase;

        if (slot.isDeleteHit()) {
            PropertyOffset newOffset = invalidOffset;
            Structure* newStructure = Structure::removePropertyTransitionFromExistingStructureConcurrently(oldStructure, propertyName.uid(), newOffset);
            if (!newStructure)
                return RetryCacheLater;
            if (!newStructure->propertyAccessesAreCacheable() || newStructure->isDictionary())
                return GiveUpOnCache;
            ASSERT(newOffset == slot.cachedOffset());
            ASSERT(newStructure->previousID() == oldStructure);
            ASSERT(newStructure->transitionKind() == TransitionKind::PropertyDeletion);
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
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
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
        if (kind == DelByKind::ById)
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdGeneric);
        else
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValGeneric);
    }
}

inline FunctionPtr<CFunctionPtrTag> appropriateOptimizingInByFunction(InByKind kind)
{
    switch (kind) {
    case InByKind::ById:
        return operationInByIdOptimize;
    case InByKind::ByVal:
        return operationInByValOptimize;
    case InByKind::PrivateName:
        return operationHasPrivateNameOptimize;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline FunctionPtr<CFunctionPtrTag> appropriateGenericInByFunction(InByKind kind)
{
    switch (kind) {
    case InByKind::ById:
        return operationInByIdGeneric;
    case InByKind::ByVal:
        return operationInByValGeneric;
    case InByKind::PrivateName:
        return operationHasPrivateNameGeneric;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static InlineCacheAction tryCacheInBy(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier propertyName,
    bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo, InByKind kind)
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
        
        RefPtr<PolyProtoAccessChain> prototypeAccessChain;
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
                    LOG_IC((ICEvent::InBySelfPatch, structure->classInfo(), ident, slot.slotBase() == base));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    repatchSlowPathCall(codeBlock, stubInfo, operationInByIdOptimize);
                    stubInfo.initInByIdSelf(locker, codeBlock, structure, slot.cachedOffset(), propertyName);
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
                    prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, base, slot);
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
                prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, base, slot);
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

        Ref<AccessCase> newCase = AccessCase::create(
            vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, propertyName, wasFound ? slot.cachedOffset() : invalidOffset, structure, conditionSet, WTFMove(prototypeAccessChain));

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfo(), ident, slot.slotBase() == base));

            RELEASE_ASSERT(result.code());
            if (kind == InByKind::ById)
                InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
            else
                InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchInBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier propertyName, bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo, InByKind kind)
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = globalObject->vm();

    if (tryCacheInBy(globalObject, codeBlock, baseObject, propertyName, wasFound, slot, stubInfo, kind) == GiveUpOnCache) {
        LOG_IC((ICEvent::InReplaceWithGeneric, baseObject->classInfo(globalObject->vm()), Identifier::fromUid(vm, propertyName.uid())));
        repatchSlowPathCall(codeBlock, stubInfo, appropriateGenericInByFunction(kind));
    }
}

static InlineCacheAction tryCacheHasPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier brandID, bool wasFound, StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;

        Structure* structure = base->structure(vm);

        InlineCacheAction action = actionForCell(vm, base);
        if (action != AttemptToCache)
            return action;

        bool isBaseProperty = true;
        LOG_IC((ICEvent::InAddAccessCase, structure->classInfo(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::create(vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, brandID, invalidOffset, structure, { }, { });

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfo(), ident, isBaseProperty));

            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchHasPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier brandID, bool wasFound, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheHasPrivateBrand(globalObject, codeBlock, baseObject, brandID, wasFound, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationHasPrivateBrandGeneric);
}

static InlineCacheAction tryCacheCheckPrivateBrand(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier brandID,
    StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;

        Structure* structure = base->structure(vm);

        InlineCacheAction action = actionForCell(vm, base);
        if (action != AttemptToCache)
            return action;

        bool isBaseProperty = true;
        LOG_IC((ICEvent::CheckPrivateBrandAddAccessCase, structure->classInfo(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::createCheckPrivateBrand(vm, codeBlock, brandID, structure);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::CheckPrivateBrandReplaceWithJump, structure->classInfo(), ident, isBaseProperty));

            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchCheckPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier brandID, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheCheckPrivateBrand(globalObject, codeBlock, baseObject, brandID, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationCheckPrivateBrandGeneric);
}

static InlineCacheAction tryCacheSetPrivateBrand(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, Structure* oldStructure, CacheableIdentifier brandID,
    StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm.heap);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        ASSERT(oldStructure);

        if (oldStructure->isDictionary())
            return RetryCacheLater;

        InlineCacheAction action = actionForCell(vm, base);
        if (action != AttemptToCache)
            return action;
        
        Structure* newStructure = Structure::setBrandTransitionFromExistingStructureConcurrently(oldStructure, brandID.uid());
        if (!newStructure)
            return RetryCacheLater;
        if (newStructure->isDictionary())
            return GiveUpOnCache;
        ASSERT(newStructure->previousID() == oldStructure);
        ASSERT(newStructure->transitionKind() == TransitionKind::SetBrand);
        ASSERT(newStructure->isObject());
        
        bool isBaseProperty = true;
        LOG_IC((ICEvent::SetPrivateBrandAddAccessCase, oldStructure->classInfo(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::createSetPrivateBrand(vm, codeBlock, brandID, oldStructure, newStructure);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::SetPrivateBrandReplaceWithJump, oldStructure->classInfo(), ident, isBaseProperty));
            
            RELEASE_ASSERT(result.code());
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchSetPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, Structure* oldStructure, CacheableIdentifier brandID, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheSetPrivateBrand(globalObject, codeBlock, baseObject, oldStructure,  brandID, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationSetPrivateBrandGeneric);
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
        RefPtr<AccessCase> newCase;
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
            InlineAccess::rewireStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo, CodeLocationLabel<JITStubRoutinePtrTag>(result.code()));
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
        repatchSlowPathCall(codeBlock, stubInfo, operationInstanceOfGeneric);
}

static void linkSlowPathTo(VM&, CallLinkInfo& callLinkInfo, MacroAssemblerCodeRef<JITStubRoutinePtrTag> codeRef)
{
    callLinkInfo.setSlowPathCallDestination(codeRef.code().template retagged<JSEntryPtrTag>());
}

static void linkSlowPathTo(VM& vm, CallLinkInfo& callLinkInfo, ThunkGenerator generator)
{
    linkSlowPathTo(vm, callLinkInfo, vm.getCTIStub(generator).retagged<JITStubRoutinePtrTag>());
}

static void linkSlowFor(VM& vm, CallLinkInfo& callLinkInfo)
{
    MacroAssemblerCodeRef<JITStubRoutinePtrTag> virtualThunk = virtualThunkFor(vm, callLinkInfo);
    linkSlowPathTo(vm, callLinkInfo, virtualThunk);
    callLinkInfo.setSlowStub(GCAwareJITStubRoutine::create(vm, virtualThunk));
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

void linkMonomorphicCall(
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
    callLinkInfo.setMonomorphicCallee(vm, owner, callee, codePtr);
    callLinkInfo.setLastSeenCallee(vm, owner, callee);

    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(callerFrame, &callLinkInfo);

    if (callLinkInfo.specializationKind() == CodeForCall && callLinkInfo.allowStubs()) {
        linkSlowPathTo(vm, callLinkInfo, linkPolymorphicCallThunkGenerator);
        return;
    }
    
    linkSlowFor(vm, callLinkInfo);
}

void linkDirectCall(
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

    callLinkInfo.setDirectCallTarget(CodeLocationLabel<JSEntryPtrTag>(codePtr));

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
        if (!callLinkInfo.clearedByJettison())
            callLinkInfo.initializeDirectCall();
    } else {
        if (!callLinkInfo.clearedByJettison()) {
            linkSlowPathTo(vm, callLinkInfo, codeRef);

            if (callLinkInfo.stub())
                callLinkInfo.revertCallToStub();
        }
        callLinkInfo.clearCallee(); // This also clears the inline cache both for data and code-based caches.
    }
    callLinkInfo.clearSeen();
    callLinkInfo.clearStub();
    callLinkInfo.clearSlowStub();
    if (callLinkInfo.isOnList())
        callLinkInfo.remove();
}

void unlinkCall(VM& vm, CallLinkInfo& callLinkInfo)
{
    dataLogLnIf(Options::dumpDisassembly(), "Unlinking call at ", callLinkInfo.fastPathStart());
    
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
    callLinkInfo.setSlowStub(GCAwareJITStubRoutine::create(vm, virtualThunk));
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

    bool isDataIC = callLinkInfo.isDataIC();
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

        bool needsDoneJump = false;
        if (frameShuffler) {
            CallFrameShuffler(stubJit, frameShuffler->snapshot()).prepareForTailCall();
            calls[caseIndex].call = stubJit.nearTailCall();
        } else if (callLinkInfo.isTailCall()) {
            stubJit.prepareForTailCallSlow();
            calls[caseIndex].call = stubJit.nearTailCall();
        } else {
            if (isDataIC)
                calls[caseIndex].call = stubJit.nearTailCall();
            else {
                calls[caseIndex].call = stubJit.nearCall();
                needsDoneJump = true;
            }
        }
        calls[caseIndex].codePtr = codePtr;
        if (needsDoneJump)
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
    if (isDataIC && !callLinkInfo.isTailCall()) {
        // We were called from the fast path, get rid of any remnants of that
        // which may exist. This really only matters for x86, which adjusts
        // SP for calls.
        stubJit.preserveReturnAddressAfterCall(GPRInfo::regT4);
    }
    stubJit.move(CCallHelpers::TrustedImmPtr(callLinkInfo.doneLocation().untaggedExecutableAddress()), GPRInfo::regT4);
    
    stubJit.restoreReturnAddressBeforeReturn(GPRInfo::regT4);
    AssemblyHelpers::Jump slow = stubJit.jump();
        
    LinkBuffer patchBuffer(stubJit, owner, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
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

    if (!done.empty())
        patchBuffer.link(done, callLinkInfo.doneLocation());
    patchBuffer.link(slow, CodeLocationLabel<JITThunkPtrTag>(vm.getCTIStub(linkPolymorphicCallThunkGenerator).code()));
    
    auto stubRoutine = adoptRef(*new PolymorphicCallStubRoutine(
        FINALIZE_CODE_FOR(
            callerCodeBlock, patchBuffer, JITStubRoutinePtrTag,
            "Polymorphic call stub for %s, return point %p, targets %s",
                isWebAssembly ? "WebAssembly" : toCString(*callerCodeBlock).data(), callLinkInfo.doneLocation().executableAddress(),
                toCString(listDump(callCases)).data()),
        vm, owner, callFrame->callerFrame(), callLinkInfo, callCases,
        WTFMove(fastCounts)));

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
    repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingGetByFunction(kind));
    switch (kind) {
    case GetByKind::ById:
    case GetByKind::ByIdWithThis:
    case GetByKind::TryById:
    case GetByKind::ByIdDirect:
    case GetByKind::PrivateNameById:
        InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
        break;
    case GetByKind::ByVal:
    case GetByKind::PrivateName:
        InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
        break;
    }
}

void resetPutBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, PutByKind kind)
{
    FunctionPtr<CFunctionPtrTag> optimizedFunction;
    switch (kind) {
    case PutByKind::ById: {
        using FunctionType = decltype(&operationPutByIdDirectStrictOptimize);
        FunctionType unoptimizedFunction = reinterpret_cast<FunctionType>(readPutICCallTarget(codeBlock, stubInfo).executableAddress());
        if (unoptimizedFunction == operationPutByIdStrict || unoptimizedFunction == operationPutByIdStrictOptimize)
            optimizedFunction = operationPutByIdStrictOptimize;
        else if (unoptimizedFunction == operationPutByIdNonStrict || unoptimizedFunction == operationPutByIdNonStrictOptimize)
            optimizedFunction = operationPutByIdNonStrictOptimize;
        else if (unoptimizedFunction == operationPutByIdDirectStrict || unoptimizedFunction == operationPutByIdDirectStrictOptimize)
            optimizedFunction = operationPutByIdDirectStrictOptimize;
        else if (unoptimizedFunction == operationPutByIdSetPrivateFieldStrict || unoptimizedFunction == operationPutByIdSetPrivateFieldStrictOptimize)
            optimizedFunction = operationPutByIdSetPrivateFieldStrictOptimize;
        else if (unoptimizedFunction == operationPutByIdDefinePrivateFieldStrict || unoptimizedFunction == operationPutByIdDefinePrivateFieldStrictOptimize)
            optimizedFunction = operationPutByIdDefinePrivateFieldStrictOptimize;
        else {
            ASSERT(unoptimizedFunction == operationPutByIdDirectNonStrict || unoptimizedFunction == operationPutByIdDirectNonStrictOptimize);
            optimizedFunction = operationPutByIdDirectNonStrictOptimize;
        }
        break;
    }
    case PutByKind::ByVal: {
        using FunctionType = decltype(&operationPutByValStrictOptimize);
        FunctionType unoptimizedFunction = reinterpret_cast<FunctionType>(readPutICCallTarget(codeBlock, stubInfo).executableAddress());
        if (unoptimizedFunction == operationPutByValStrictGeneric || unoptimizedFunction == operationPutByValStrictOptimize)
            optimizedFunction = operationPutByValStrictOptimize;
        else if (unoptimizedFunction == operationPutByValNonStrictGeneric || unoptimizedFunction == operationPutByValNonStrictOptimize)
            optimizedFunction = operationPutByValNonStrictOptimize;
        else if (unoptimizedFunction == operationDirectPutByValStrictGeneric || unoptimizedFunction == operationDirectPutByValStrictOptimize)
            optimizedFunction = operationDirectPutByValStrictOptimize;
        else if (unoptimizedFunction == operationPutByValDefinePrivateFieldGeneric || unoptimizedFunction == operationPutByValDefinePrivateFieldOptimize)
            optimizedFunction = operationPutByValDefinePrivateFieldOptimize;
        else if (unoptimizedFunction == operationPutByValSetPrivateFieldGeneric || unoptimizedFunction == operationPutByValSetPrivateFieldOptimize)
            optimizedFunction = operationPutByValSetPrivateFieldOptimize;
        else {
            ASSERT(unoptimizedFunction == operationDirectPutByValNonStrictGeneric || unoptimizedFunction == operationDirectPutByValNonStrictOptimize);
            optimizedFunction = operationDirectPutByValNonStrictOptimize;
        }
        break;
    }
    }

    repatchSlowPathCall(codeBlock, stubInfo, optimizedFunction);
    switch (kind) {
    case PutByKind::ById:
        InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
        break;
    case PutByKind::ByVal:
        InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
        break;
    }
}

void resetDelBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, DelByKind kind)
{
    if (kind == DelByKind::ById)
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdOptimize);
    else
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValOptimize);
    InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
}

void resetInBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, InByKind kind)
{
    repatchSlowPathCall(codeBlock, stubInfo, appropriateOptimizingInByFunction(kind));
    if (kind == InByKind::ById)
        InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
    else
        InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
}

void resetHasPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationHasPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
}

void resetInstanceOf(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationInstanceOfOptimize);
    InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
}

void resetCheckPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationCheckPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
}

void resetSetPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationSetPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccessNotUsingInlineAccess(codeBlock, stubInfo);
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
