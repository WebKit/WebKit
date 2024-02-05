/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#include "BinarySwitch.h"
#include "CCallHelpers.h"
#include "CacheableIdentifierInlines.h"
#include "CallFrameShuffler.h"
#include "DFGOperations.h"
#include "DFGSpeculativeJIT.h"
#include "DOMJITGetterSetter.h"
#include "DirectArguments.h"
#include "ECMAMode.h"
#include "ExecutableBaseInlines.h"
#include "FTLThunks.h"
#include "FullCodeOrigin.h"
#include "FunctionCodeBlock.h"
#include "GCAwareJITStubRoutine.h"
#include "GetterSetter.h"
#include "GetterSetterAccessCase.h"
#include "ICStats.h"
#include "InlineAccess.h"
#include "InlineCacheCompiler.h"
#include "InstanceOfAccessCase.h"
#include "IntrinsicGetterAccessCase.h"
#include "JIT.h"
#include "JITInlines.h"
#include "JITThunks.h"
#include "JSCInlines.h"
#include "JSModuleNamespaceObject.h"
#include "JSWebAssembly.h"
#include "JSWebAssemblyInstance.h"
#include "JSWebAssemblyModule.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "ModuleNamespaceAccessCase.h"
#include "ProxyObjectAccessCase.h"
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

#if ENABLE(JIT)

static ECMAMode ecmaModeFor(PutByKind putByKind)
{
    switch (putByKind) {
    case PutByKind::ByIdSloppy:
    case PutByKind::ByValSloppy:
    case PutByKind::ByIdDirectSloppy:
    case PutByKind::ByValDirectSloppy:
        return ECMAMode::sloppy();

    case PutByKind::ByIdStrict:
    case PutByKind::ByValStrict:
    case PutByKind::ByIdDirectStrict:
    case PutByKind::ByValDirectStrict:
    case PutByKind::DefinePrivateNameById:
    case PutByKind::DefinePrivateNameByVal:
    case PutByKind::SetPrivateNameById:
    case PutByKind::SetPrivateNameByVal:
        return ECMAMode::strict();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static void linkSlowFor(VM& vm, CallLinkInfo& callLinkInfo)
{
    if (callLinkInfo.type() == CallLinkInfo::Type::Optimizing)
        static_cast<OptimizingCallLinkInfo&>(callLinkInfo).setSlowPathCallDestination(vm.getCTIVirtualCall(callLinkInfo.callMode()).code().template retagged<JSEntryPtrTag>());
}
#endif

void linkMonomorphicCall(VM& vm, JSCell* owner, CallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock, JSObject* callee, CodePtr<JSEntryPtrTag> codePtr)
{
    ASSERT(!callLinkInfo.stub());

    CodeBlock* callerCodeBlock = jsDynamicCast<CodeBlock*>(owner); // WebAssembly -> JS stubs don't have a valid CodeBlock.
    ASSERT(owner);

    ASSERT(!callLinkInfo.isLinked());
    callLinkInfo.setMonomorphicCallee(vm, owner, callee, calleeCodeBlock, codePtr);
    callLinkInfo.setLastSeenCallee(vm, owner, callee);

    if (shouldDumpDisassemblyFor(callerCodeBlock))
        dataLog("Linking call in ", FullCodeOrigin(callerCodeBlock, callLinkInfo.codeOrigin()), " to ", pointerDump(calleeCodeBlock), ", entrypoint at ", codePtr, "\n");

    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(owner, &callLinkInfo);

#if ENABLE(JIT)
    if (callLinkInfo.specializationKind() == CodeForCall && callLinkInfo.allowStubs())
        return;
    linkSlowFor(vm, callLinkInfo);
#endif
}

CodePtr<JSEntryPtrTag> jsToWasmICCodePtr(CodeSpecializationKind kind, JSObject* callee)
{
#if ENABLE(WEBASSEMBLY)
    if (!callee)
        return nullptr;
    if (kind != CodeForCall)
        return nullptr;
    if (auto* wasmFunction = jsDynamicCast<WebAssemblyFunction*>(callee))
        return wasmFunction->jsCallEntrypoint();
#else
    UNUSED_PARAM(kind);
    UNUSED_PARAM(callee);
#endif
    return nullptr;
}

#if ENABLE(JIT)

void ftlThunkAwareRepatchCall(CodeBlock* codeBlock, CodeLocationCall<JSInternalPtrTag> call, CodePtr<CFunctionPtrTag> newCalleeFunction)
{
#if ENABLE(FTL_JIT)
    if (codeBlock->jitType() == JITType::FTLJIT) {
        VM& vm = codeBlock->vm();
        FTL::Thunks& thunks = *vm.ftlThunks;
        CodePtr<JITThunkPtrTag> slowPathThunk = MacroAssembler::readCallTarget<JITThunkPtrTag>(call);
        FTL::SlowPathCallKey key = thunks.keyForSlowPathCallThunk(slowPathThunk);
        key = key.withCallTarget(newCalleeFunction);
        MacroAssembler::repatchCall(call, thunks.getSlowPathCallThunk(vm, key).code());
        return;
    }
#else // ENABLE(FTL_JIT)
    UNUSED_PARAM(codeBlock);
#endif // ENABLE(FTL_JIT)
    MacroAssembler::repatchCall(call, newCalleeFunction.retagged<OperationPtrTag>());
}

static void repatchSlowPathCall(CodeBlock* codeBlock, StructureStubInfo& stubInfo, CodePtr<CFunctionPtrTag> newCalleeFunction)
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
    AttemptToCache,
    PromoteToMegamorphic,
};

static InlineCacheAction actionForCell(VM& vm, JSCell* cell)
{
    Structure* structure = cell->structure();

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
            GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
            stubInfo.reset(locker, codeBlock);
        }
    }
}

inline CodePtr<CFunctionPtrTag> appropriateGetByOptimizeFunction(GetByKind kind)
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
    case GetByKind::ByValWithThis:
        return operationGetByValWithThisOptimize;
    case GetByKind::PrivateName:
        return operationGetPrivateNameOptimize;
    case GetByKind::PrivateNameById:
        return operationGetPrivateNameByIdOptimize;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline CodePtr<CFunctionPtrTag> appropriateGetByGaveUpFunction(GetByKind kind)
{
    switch (kind) {
    case GetByKind::ById:
        return operationGetByIdGaveUp;
    case GetByKind::ByIdWithThis:
        return operationGetByIdWithThisGaveUp;
    case GetByKind::TryById:
        return operationTryGetByIdGaveUp;
    case GetByKind::ByIdDirect:
        return operationGetByIdDirectGaveUp;
    case GetByKind::ByVal:
        return operationGetByValGaveUp;
    case GetByKind::ByValWithThis:
        return operationGetByValWithThisGaveUp;
    case GetByKind::PrivateName:
        return operationGetPrivateNameGaveUp;
    case GetByKind::PrivateNameById:
        return operationGetPrivateNameByIdGaveUp;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static InlineCacheAction tryCacheGetBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, CacheableIdentifier propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByKind kind)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());

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
                    && InlineAccess::isCacheableArrayLength(codeBlock, stubInfo, jsCast<JSArray*>(baseCell))) {

                    bool generatedCodeInline = InlineAccess::generateArrayLength(codeBlock, stubInfo, jsCast<JSArray*>(baseCell));
                    if (generatedCodeInline) {
                        repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByOptimizeFunction(kind));
                        stubInfo.initArrayLength(locker);
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::ArrayLength, propertyName);
            } else if (isJSString(baseCell)) {
                if (stubInfo.cacheType() == CacheType::Unset
                    && InlineAccess::isCacheableStringLength(codeBlock, stubInfo)) {
                    bool generatedCodeInline = InlineAccess::generateStringLength(codeBlock, stubInfo);
                    if (generatedCodeInline) {
                        repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByOptimizeFunction(kind));
                        stubInfo.initStringLength(locker);
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::create(vm, codeBlock, AccessCase::StringLength, propertyName);
            } else if (DirectArguments* arguments = jsDynamicCast<DirectArguments*>(baseCell)) {
                // If there were overrides, then we can handle this as a normal property load! Guarding
                // this with such a check enables us to add an IC case for that load if needed.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::DirectArgumentsLength, propertyName);
            } else if (ScopedArguments* arguments = jsDynamicCast<ScopedArguments*>(baseCell)) {
                // Ditto.
                if (!arguments->overrodeThings())
                    newCase = AccessCase::create(vm, codeBlock, AccessCase::ScopedArgumentsLength, propertyName);
            }
        }

        if (!propertyName.isSymbol() && baseCell->inherits<JSModuleNamespaceObject>() && !slot.isUnset()) {
            if (auto moduleNamespaceSlot = slot.moduleNamespaceSlot())
                newCase = ModuleNamespaceAccessCase::create(vm, codeBlock, propertyName, jsCast<JSModuleNamespaceObject*>(baseCell), moduleNamespaceSlot->environment, ScopeOffset(moduleNamespaceSlot->scopeOffset));
        }

        if (!propertyName.isPrivateName() && baseCell->inherits<ProxyObject>()) {
            switch (kind) {
            case GetByKind::ById:
            case GetByKind::ByIdWithThis: {
                propertyName.ensureIsCell(vm);
                newCase = ProxyObjectAccessCase::create(vm, codeBlock, AccessCase::ProxyObjectLoad, propertyName);
                break;
            }
            case GetByKind::ByVal:
            case GetByKind::ByValWithThis: {
                newCase = ProxyObjectAccessCase::create(vm, codeBlock, AccessCase::IndexedProxyObjectLoad, nullptr);
                break;
            }
            default:
                break;
            }
        }

        if (!newCase) {
            if (!slot.isCacheable() && !slot.isUnset())
                return GiveUpOnCache;

            ObjectPropertyConditionSet conditionSet;
            Structure* structure = baseCell->structure();

            bool loadTargetFromProxy = false;
            if (baseCell->type() == GlobalProxyType) {
                if (isPrivate)
                    return GiveUpOnCache;
                baseValue = jsCast<JSGlobalProxy*>(baseCell)->target();
                baseCell = baseValue.asCell();
                structure = baseCell->structure();
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
                bool generatedCodeInline = InlineAccess::generateSelfPropertyAccess(codeBlock, stubInfo, structure, slot.cachedOffset());
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::GetBySelfPatch, structure->classInfoForCells(), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByOptimizeFunction(kind));
                    stubInfo.initGetByIdSelf(locker, codeBlock, structure, slot.cachedOffset());
                    return RetryCacheLater;
                }
            }

            RefPtr<PolyProtoAccessChain> prototypeAccessChain;

            PropertyOffset offset = slot.isUnset() ? invalidOffset : slot.cachedOffset();

            if (slot.isCustom() && slot.slotBase() == baseValue) {
                // To cache self customs, we must disallow dictionaries because we
                // need to be informed if the custom goes away since we cache the
                // constant function pointer.

                if (!prepareChainForCaching(globalObject, slot.slotBase(), propertyName.uid(), slot.slotBase()))
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
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, propertyName.uid(), slot);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->flattenedDictionary) {
                        // Property offsets may have changed due to flattening. We'll cache later.
                        return RetryCacheLater;
                    }

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, propertyName, slot);
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                        ASSERT(slot.isCacheableCustom() || prototypeAccessChain->slotBaseStructure(vm, structure)->get(vm, propertyName.uid()) == offset);
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
                getter = jsDynamicCast<JSFunction*>(slot.getterSetter()->getter());

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
            } else if (!loadTargetFromProxy && getter && InlineCacheCompiler::canEmitIntrinsicGetter(stubInfo, getter, structure))
                newCase = IntrinsicGetterAccessCase::create(vm, codeBlock, propertyName, slot.cachedOffset(), structure, conditionSet, getter, WTFMove(prototypeAccessChain));
            else {
                if (isPrivate) {
                    RELEASE_ASSERT(!slot.isUnset());
                    constexpr bool isGlobalProxy = false;
                    if (!slot.isCacheable())
                        return GiveUpOnCache;
                    newCase = ProxyableAccessCase::create(vm, codeBlock, AccessCase::Load, propertyName, offset, structure,
                        conditionSet, isGlobalProxy, slot.watchpointSet(), WTFMove(prototypeAccessChain));
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

                    if ((kind == GetByKind::ByIdWithThis || kind == GetByKind::ByValWithThis) && type == AccessCase::CustomAccessorGetter && domAttribute)
                        return GiveUpOnCache;

                    CodePtr<CustomAccessorPtrTag> customAccessor;
                    if (slot.isCacheableCustom())
                        customAccessor = slot.customGetter();
                    newCase = GetterSetterAccessCase::create(
                        vm, codeBlock, type, propertyName, offset, structure, conditionSet, loadTargetFromProxy,
                        slot.watchpointSet(), customAccessor,
                        slot.isCacheableCustom() && slot.slotBase() != baseValue ? slot.slotBase() : nullptr,
                        domAttribute, WTFMove(prototypeAccessChain));
                }
            }
        }

        LOG_IC((ICEvent::GetByAddAccessCase, baseValue.classInfoOrNull(), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::GetByReplaceWithJump, baseValue.classInfoOrNull(), Identifier::fromUid(vm, propertyName.uid()), slot.slotBase() == baseValue));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    if (result.generatedMegamorphicCode())
        return PromoteToMegamorphic;
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchGetBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, CacheableIdentifier propertyName, const PropertySlot& slot, StructureStubInfo& stubInfo, GetByKind kind)
{
    SuperSamplerScope superSamplerScope(false);

    switch (tryCacheGetBy(globalObject, codeBlock, baseValue, propertyName, slot, stubInfo, kind)) {
    case PromoteToMegamorphic: {
        switch (kind) {
        case GetByKind::ById:
            repatchSlowPathCall(codeBlock, stubInfo, operationGetByIdMegamorphic);
            break;
        case GetByKind::ByIdWithThis:
            repatchSlowPathCall(codeBlock, stubInfo, operationGetByIdWithThisMegamorphic);
            break;
        case GetByKind::ByVal:
            repatchSlowPathCall(codeBlock, stubInfo, operationGetByValMegamorphic);
            break;
        case GetByKind::ByValWithThis:
            repatchSlowPathCall(codeBlock, stubInfo, operationGetByValWithThisMegamorphic);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
    case GiveUpOnCache:
        repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByGaveUpFunction(kind));
        break;
    case RetryCacheLater:
    case AttemptToCache:
        break;
    }
}

// Mainly used to transition from megamorphic case to generic case.
void repatchGetBySlowPathCall(CodeBlock* codeBlock, StructureStubInfo& stubInfo, GetByKind kind)
{
    resetGetBy(codeBlock, stubInfo, kind);
    repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByGaveUpFunction(kind));
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
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());

        JSCell* base = baseValue.asCell();

        RefPtr<AccessCase> newCase;
        AccessCase::AccessType accessType = AccessCase::IndexedInt32Load;
        if (base->type() == DirectArgumentsType)
            accessType = AccessCase::IndexedDirectArgumentsLoad;
        else if (base->type() == ScopedArgumentsType)
            accessType = AccessCase::IndexedScopedArgumentsLoad;
        else if (base->type() == StringType)
            accessType = AccessCase::IndexedStringLoad;
        else if (isTypedView(base->type())) {
            auto* typedArray = jsCast<JSArrayBufferView*>(base);
#if USE(JSVALUE32_64)
            if (typedArray->isResizableOrGrowableShared())
                return GiveUpOnCache;
#endif
            switch (typedArray->type()) {
            case Int8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt8Load : AccessCase::IndexedTypedArrayInt8Load;
                break;
            case Uint8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8Load : AccessCase::IndexedTypedArrayUint8Load;
                break;
            case Uint8ClampedArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8ClampedLoad : AccessCase::IndexedTypedArrayUint8ClampedLoad;
                break;
            case Int16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt16Load : AccessCase::IndexedTypedArrayInt16Load;
                break;
            case Uint16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint16Load : AccessCase::IndexedTypedArrayUint16Load;
                break;
            case Int32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt32Load : AccessCase::IndexedTypedArrayInt32Load;
                break;
            case Uint32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint32Load : AccessCase::IndexedTypedArrayUint32Load;
                break;
            case Float32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat32Load : AccessCase::IndexedTypedArrayFloat32Load;
                break;
            case Float64ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat64Load : AccessCase::IndexedTypedArrayFloat64Load;
                break;
            // FIXME: Optimize BigInt64Array / BigUint64Array in IC
            // https://bugs.webkit.org/show_bug.cgi?id=221183
            case BigInt64ArrayType:
            case BigUint64ArrayType:
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
                ASSERT(Options::allowDoubleShape());
                accessType = AccessCase::IndexedDoubleLoad;
                break;
            case ContiguousShape:
                accessType = AccessCase::IndexedContiguousLoad;
                break;
            case ArrayStorageShape:
                accessType = AccessCase::IndexedArrayStorageLoad;
                break;
            case NoIndexingShape: {
                if (!base->isObject())
                    return GiveUpOnCache;

                if (base->structure()->mayInterceptIndexedAccesses() || base->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
                    return GiveUpOnCache;

                // FIXME: prepareChainForCaching is conservative. We should have another function which only cares about information related to this IC.
                auto cacheStatus = prepareChainForCaching(globalObject, base, nullptr, nullptr);
                if (!cacheStatus)
                    return GiveUpOnCache;

                if (cacheStatus->usesPolyProto)
                    return GiveUpOnCache;

                Structure* headStructure = base->structure();
                ObjectPropertyConditionSet conditionSet = generateConditionsForIndexedMiss(vm, codeBlock, globalObject, headStructure);
                if (!conditionSet.isValid())
                    return GiveUpOnCache;

                newCase = AccessCase::create(vm, codeBlock, AccessCase::IndexedNoIndexingMiss, nullptr, invalidOffset, headStructure, conditionSet);
                break;
            }
            default:
                return GiveUpOnCache;
            }
        }

        if (!newCase)
            newCase = AccessCase::create(vm, codeBlock, accessType, nullptr);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, newCase.releaseNonNull());

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::GetByReplaceWithJump, baseValue.classInfoOrNull(), Identifier()));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayGetByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo, GetByKind kind)
{
    if (tryCacheArrayGetByVal(globalObject, codeBlock, base, index, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByGaveUpFunction(kind));
}

static CodePtr<CFunctionPtrTag> appropriatePutByGaveUpFunction(PutByKind putByKind)
{
    switch (putByKind) {
    case PutByKind::ByIdStrict:
        return operationPutByIdStrictGaveUp;
    case PutByKind::ByIdSloppy:
        return operationPutByIdSloppyGaveUp;
    case PutByKind::ByIdDirectStrict:
        return operationPutByIdDirectStrictGaveUp;
    case PutByKind::ByIdDirectSloppy:
        return operationPutByIdDirectSloppyGaveUp;
    case PutByKind::DefinePrivateNameById:
        return operationPutByIdDefinePrivateFieldStrictGaveUp;
    case PutByKind::SetPrivateNameById:
        return operationPutByIdSetPrivateFieldStrictGaveUp;
    case PutByKind::ByValStrict:
        return operationPutByValStrictGaveUp;
    case PutByKind::ByValSloppy:
        return operationPutByValSloppyGaveUp;
    case PutByKind::ByValDirectStrict:
        return operationDirectPutByValStrictGaveUp;
    case PutByKind::ByValDirectSloppy:
        return operationDirectPutByValSloppyGaveUp;
    case PutByKind::DefinePrivateNameByVal:
        return operationPutByValDefinePrivateFieldGaveUp;
    case PutByKind::SetPrivateNameByVal:
        return operationPutByValSetPrivateFieldGaveUp;
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

// Mainly used to transition from megamorphic case to generic case.
void repatchPutBySlowPathCall(CodeBlock* codeBlock, StructureStubInfo& stubInfo, PutByKind kind)
{
    resetPutBy(codeBlock, stubInfo, kind);
    repatchSlowPathCall(codeBlock, stubInfo, appropriatePutByGaveUpFunction(kind));
}

static CodePtr<CFunctionPtrTag> appropriatePutByOptimizeFunction(PutByKind putByKind)
{
    switch (putByKind) {
    case PutByKind::ByIdStrict:
        return operationPutByIdStrictOptimize;
    case PutByKind::ByIdSloppy:
        return operationPutByIdSloppyOptimize;
    case PutByKind::ByIdDirectStrict:
        return operationPutByIdDirectStrictOptimize;
    case PutByKind::ByIdDirectSloppy:
        return operationPutByIdDirectSloppyOptimize;
    case PutByKind::DefinePrivateNameById:
        return operationPutByIdDefinePrivateFieldStrictOptimize;
    case PutByKind::SetPrivateNameById:
        return operationPutByIdSetPrivateFieldStrictOptimize;
    case PutByKind::ByValStrict:
        return operationPutByValStrictOptimize;
    case PutByKind::ByValSloppy:
        return operationPutByValSloppyOptimize;
    case PutByKind::ByValDirectStrict:
        return operationDirectPutByValStrictOptimize;
    case PutByKind::ByValDirectSloppy:
        return operationDirectPutByValSloppyOptimize;
    case PutByKind::DefinePrivateNameByVal:
        return operationPutByValDefinePrivateFieldOptimize;
    case PutByKind::SetPrivateNameByVal:
        return operationPutByValSetPrivateFieldOptimize;
    }
    // Make win port compiler happy
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
}

static InlineCacheAction tryCachePutBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutByKind putByKind)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, propertyName.uid());
    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());

        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        if (!baseValue.isCell())
            return GiveUpOnCache;

        JSCell* baseCell = baseValue.asCell();

        bool isProxyObject = baseCell->type() == ProxyObjectType;
        if (!isProxyObject) {
            if (!slot.isCacheablePut() && !slot.isCacheableCustom() && !slot.isCacheableSetter())
                return GiveUpOnCache;

            // FIXME: We should try to do something smarter here...
            if (isCopyOnWrite(oldStructure->indexingMode()))
                return GiveUpOnCache;
            // We can't end up storing to a CoW on the prototype since it shouldn't own properties.
            ASSERT(!isCopyOnWrite(slot.base()->indexingMode()));

            if (!oldStructure->propertyAccessesAreCacheable())
                return GiveUpOnCache;
        }
        
        bool isGlobalProxy = false;
        if (baseCell->type() == GlobalProxyType) {
            baseCell = jsCast<JSGlobalProxy*>(baseCell)->target();
            baseValue = baseCell;
            isGlobalProxy = true;

            // We currently only cache Replace and JS/Custom Setters on JSGlobalProxy. We don't
            // cache transitions because global objects will never share the same structure
            // in our current implementation.
            bool isCacheableProxy = (slot.isCacheablePut() && slot.type() == PutPropertySlot::ExistingProperty)
                || slot.isCacheableSetter()
                || slot.isCacheableCustom();
            if (!isCacheableProxy)
                return GiveUpOnCache;
        }

        if (isGlobalProxy) {
            switch (putByKind) {
            case PutByKind::DefinePrivateNameById:
            case PutByKind::DefinePrivateNameByVal:
            case PutByKind::SetPrivateNameById:
            case PutByKind::SetPrivateNameByVal:
                return GiveUpOnCache;
            case PutByKind::ByIdStrict:
            case PutByKind::ByIdSloppy:
            case PutByKind::ByValStrict:
            case PutByKind::ByValSloppy:
            case PutByKind::ByIdDirectStrict:
            case PutByKind::ByIdDirectSloppy:
            case PutByKind::ByValDirectStrict:
            case PutByKind::ByValDirectSloppy:
                break;
            }
        }

        RefPtr<AccessCase> newCase;

        if (slot.base() == baseValue && slot.isCacheablePut()) {
            if (slot.type() == PutPropertySlot::ExistingProperty) {
                // This assert helps catch bugs if we accidentally forget to disable caching
                // when we transition then store to an existing property. This is common among
                // paths that reify lazy properties. If we reify a lazy property and forget
                // to disable caching, we may come down this path. The Replace IC does not
                // know how to model these types of structure transitions (or any structure
                // transition for that matter).
                RELEASE_ASSERT(baseValue.asCell()->structure() == oldStructure);

                oldStructure->didCachePropertyReplacement(vm, slot.cachedOffset());
            
                if (stubInfo.cacheType() == CacheType::Unset
                    && InlineAccess::canGenerateSelfPropertyReplace(codeBlock, stubInfo, slot.cachedOffset())
                    && !oldStructure->needImpurePropertyWatchpoint()
                    && !isGlobalProxy) {
                    
                    bool generatedCodeInline = InlineAccess::generateSelfPropertyReplace(codeBlock, stubInfo, oldStructure, slot.cachedOffset());
                    if (generatedCodeInline) {
                        LOG_IC((ICEvent::PutBySelfPatch, oldStructure->classInfoForCells(), ident, slot.base() == baseValue));
                        repatchSlowPathCall(codeBlock, stubInfo, appropriatePutByOptimizeFunction(putByKind));
                        stubInfo.initPutByIdReplace(locker, codeBlock, oldStructure, slot.cachedOffset());
                        return RetryCacheLater;
                    }
                }

                newCase = AccessCase::createReplace(vm, codeBlock, propertyName, slot.cachedOffset(), oldStructure, isGlobalProxy);
            } else {
                ASSERT(!isGlobalProxy);
                ASSERT(slot.type() == PutPropertySlot::NewProperty);

                if (!oldStructure->isObject())
                    return GiveUpOnCache;

                // Right now, we disable IC for put onto prototype for NewProperty case.
                if (oldStructure->mayBePrototype())
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
                ASSERT(baseValue.asCell()->structure() == newStructure);
                if (baseValue.asCell()->structure() != newStructure)
                    return GiveUpOnCache;

                ASSERT(newStructure->previousID() == oldStructure);
                ASSERT(!newStructure->isDictionary());
                ASSERT(newStructure->isObject());
                
                RefPtr<PolyProtoAccessChain> prototypeAccessChain;
                ObjectPropertyConditionSet conditionSet;
                switch (putByKind) {
                case PutByKind::ByIdStrict:
                case PutByKind::ByIdSloppy:
                case PutByKind::ByValStrict:
                case PutByKind::ByValSloppy: {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, propertyName.uid(), nullptr);
                    if (!cacheStatus)
                        return GiveUpOnCache;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, propertyName, nullptr);
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                    } else {
                        prototypeAccessChain = nullptr;
                        conditionSet = generateConditionsForPropertySetterMiss(
                            vm, codeBlock, globalObject, newStructure, ident.impl());
                        if (!conditionSet.isValid())
                            return GiveUpOnCache;
                    }
                    break;
                }
                case PutByKind::DefinePrivateNameById:
                case PutByKind::DefinePrivateNameByVal: {
                    ASSERT(ident.isPrivateName());
                    conditionSet = generateConditionsForPropertyMiss(vm, codeBlock, globalObject, newStructure, ident.impl());
                    if (!conditionSet.isValid())
                        return GiveUpOnCache;
                    break;
                }
                case PutByKind::ByIdDirectStrict:
                case PutByKind::ByIdDirectSloppy:
                case PutByKind::ByValDirectStrict:
                case PutByKind::ByValDirectSloppy:
                case PutByKind::SetPrivateNameById:
                case PutByKind::SetPrivateNameByVal:
                    break;
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
                auto cacheStatus = prepareChainForCaching(globalObject, baseCell, propertyName.uid(), slot.base());
                if (!cacheStatus)
                    return GiveUpOnCache;

                if (slot.base() != baseValue) {
                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, propertyName, slot.base());
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
                    invalidOffset, conditionSet, WTFMove(prototypeAccessChain), isGlobalProxy, slot.customSetter(), slot.base() != baseValue ? slot.base() : nullptr);
            } else {
                ASSERT(slot.isCacheableSetter());
                ObjectPropertyConditionSet conditionSet;
                RefPtr<PolyProtoAccessChain> prototypeAccessChain;
                PropertyOffset offset = slot.cachedOffset();

                if (slot.base() != baseValue) {
                    auto cacheStatus = prepareChainForCaching(globalObject, baseCell, propertyName.uid(), slot.base());
                    if (!cacheStatus)
                        return GiveUpOnCache;
                    if (cacheStatus->flattenedDictionary)
                        return RetryCacheLater;

                    if (cacheStatus->usesPolyProto) {
                        prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, baseCell, propertyName, slot.base());
                        if (!prototypeAccessChain)
                            return GiveUpOnCache;
                        unsigned attributes;
                        offset = prototypeAccessChain->slotBaseStructure(vm, baseCell->structure())->get(vm, ident.impl(), attributes);
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
                    vm, codeBlock, AccessCase::Setter, oldStructure, propertyName, offset, conditionSet, WTFMove(prototypeAccessChain), isGlobalProxy);
            }
        } else if (!propertyName.isPrivateName() && isProxyObject) {
            propertyName.ensureIsCell(vm);
            newCase = ProxyObjectAccessCase::create(vm, codeBlock, AccessCase::ProxyObjectStore, propertyName);
        }

        LOG_IC((ICEvent::PutByAddAccessCase, oldStructure->classInfoForCells(), ident, slot.base() == baseValue));
        
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, slot.isStrictMode() ? ECMAMode::strict() : ECMAMode::sloppy(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByReplaceWithJump, oldStructure->classInfoForCells(), ident, slot.base() == baseValue));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    if (result.generatedMegamorphicCode())
        return PromoteToMegamorphic;
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchPutBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, const PutPropertySlot& slot, StructureStubInfo& stubInfo, PutByKind putByKind)
{
    SuperSamplerScope superSamplerScope(false);
    
    switch (tryCachePutBy(globalObject, codeBlock, baseValue, oldStructure, propertyName, slot, stubInfo, putByKind)) {
    case PromoteToMegamorphic: {
        switch (putByKind) {
        case PutByKind::ByIdStrict:
            repatchSlowPathCall(codeBlock, stubInfo, operationPutByIdStrictMegamorphic);
            break;
        case PutByKind::ByIdSloppy:
            repatchSlowPathCall(codeBlock, stubInfo, operationPutByIdSloppyMegamorphic);
            break;
        case PutByKind::ByValStrict:
            repatchSlowPathCall(codeBlock, stubInfo, operationPutByValStrictMegamorphic);
            break;
        case PutByKind::ByValSloppy:
            repatchSlowPathCall(codeBlock, stubInfo, operationPutByValSloppyMegamorphic);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        break;
    }
    case GiveUpOnCache:
        repatchSlowPathCall(codeBlock, stubInfo, appropriatePutByGaveUpFunction(putByKind));
        break;
    case RetryCacheLater:
    case AttemptToCache:
        break;
    }
}

static InlineCacheAction tryCacheArrayPutByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue index, StructureStubInfo& stubInfo, PutByKind putByKind)
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
        if (isTypedView(base->type())) {
            auto* typedArray = jsCast<JSArrayBufferView*>(base);
#if USE(JSVALUE32_64)
            if (typedArray->isResizableOrGrowableShared())
                return GiveUpOnCache;
#endif
            switch (typedArray->type()) {
            case Int8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt8Store : AccessCase::IndexedTypedArrayInt8Store;
                break;
            case Uint8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8Store : AccessCase::IndexedTypedArrayUint8Store;
                break;
            case Uint8ClampedArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8ClampedStore : AccessCase::IndexedTypedArrayUint8ClampedStore;
                break;
            case Int16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt16Store : AccessCase::IndexedTypedArrayInt16Store;
                break;
            case Uint16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint16Store : AccessCase::IndexedTypedArrayUint16Store;
                break;
            case Int32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt32Store : AccessCase::IndexedTypedArrayInt32Store;
                break;
            case Uint32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint32Store : AccessCase::IndexedTypedArrayUint32Store;
                break;
            case Float32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat32Store : AccessCase::IndexedTypedArrayFloat32Store;
                break;
            case Float64ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat64Store : AccessCase::IndexedTypedArrayFloat64Store;
                break;
            // FIXME: Optimize BigInt64Array / BigUint64Array in IC
            // https://bugs.webkit.org/show_bug.cgi?id=221183
            case BigInt64ArrayType:
            case BigUint64ArrayType:
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
                ASSERT(Options::allowDoubleShape());
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

        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ecmaModeFor(putByKind), nullptr, AccessCase::create(vm, codeBlock, accessType, nullptr));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::PutByReplaceWithJump, baseValue.classInfoOrNull(), Identifier()));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayPutByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo, PutByKind putByKind)
{
    if (tryCacheArrayPutByVal(globalObject, codeBlock, base, index, stubInfo, putByKind) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, appropriatePutByGaveUpFunction(putByKind));
}

static InlineCacheAction tryCacheDeleteBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, DeletePropertySlot& slot, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, StructureStubInfo& stubInfo, DelByKind, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());

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
            // Right now, we disable IC for put onto prototype.
            if (oldStructure->mayBePrototype())
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
            // Right now, we disable IC for put onto prototype.
            if (oldStructure->mayBePrototype())
                return GiveUpOnCache;
            newCase = AccessCase::create(vm, codeBlock, AccessCase::DeleteNonConfigurable, propertyName, invalidOffset, oldStructure, { }, nullptr);
        } else
            newCase = AccessCase::create(vm, codeBlock, AccessCase::DeleteMiss, propertyName, invalidOffset, oldStructure, { }, nullptr);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ecmaMode, propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::DelByReplaceWithJump, oldStructure->classInfoForCells(), Identifier::fromUid(vm, propertyName.uid())));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchDeleteBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, DeletePropertySlot& slot, JSValue baseValue, Structure* oldStructure, CacheableIdentifier propertyName, StructureStubInfo& stubInfo, DelByKind kind, ECMAMode ecmaMode)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheDeleteBy(globalObject, codeBlock, slot, baseValue, oldStructure, propertyName, stubInfo, kind, ecmaMode) == GiveUpOnCache) {
        LOG_IC((ICEvent::DelByReplaceWithGeneric, baseValue.classInfoOrNull(), Identifier::fromUid(globalObject->vm(), propertyName.uid())));
        switch (kind) {
        case DelByKind::ByIdStrict:
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdStrictGaveUp);
            break;
        case DelByKind::ByIdSloppy:
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdSloppyGaveUp);
            break;
        case DelByKind::ByValStrict:
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValStrictGaveUp);
            break;
        case DelByKind::ByValSloppy:
            repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValSloppyGaveUp);
            break;
        }
    }
}

inline CodePtr<CFunctionPtrTag> appropriateInByOptimizeFunction(InByKind kind)
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

inline CodePtr<CFunctionPtrTag> appropriateInByGaveUpFunction(InByKind kind)
{
    switch (kind) {
    case InByKind::ById:
        return operationInByIdGaveUp;
    case InByKind::ByVal:
        return operationInByValGaveUp;
    case InByKind::PrivateName:
        return operationHasPrivateNameGaveUp;
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
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;
        
        RefPtr<AccessCase> newCase;
        Structure* structure = base->structure();
        
        RefPtr<PolyProtoAccessChain> prototypeAccessChain;
        ObjectPropertyConditionSet conditionSet;
        if ((kind == InByKind::ById || kind == InByKind::ByVal) && !propertyName.isPrivateName() && base->inherits<ProxyObject>()) {
            propertyName.ensureIsCell(vm);
            newCase = ProxyObjectAccessCase::create(vm, codeBlock, AccessCase::ProxyObjectHas, propertyName);
        } else if (wasFound) {
            if (!structure->propertyAccessesAreCacheable())
                return GiveUpOnCache;

            if (!slot.isCacheable())
                return GiveUpOnCache;

            InlineCacheAction action = actionForCell(vm, base);
            if (action != AttemptToCache)
                return action;

            // Optimize self access.
            if (stubInfo.cacheType() == CacheType::Unset
                && slot.isCacheableValue()
                && slot.slotBase() == base
                && !slot.watchpointSet()
                && !structure->needImpurePropertyWatchpoint()) {
                bool generatedCodeInline = InlineAccess::generateSelfInAccess(codeBlock, stubInfo, structure);
                if (generatedCodeInline) {
                    LOG_IC((ICEvent::InBySelfPatch, structure->classInfoForCells(), ident, slot.slotBase() == base));
                    structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
                    repatchSlowPathCall(codeBlock, stubInfo, operationInByIdOptimize);
                    stubInfo.initInByIdSelf(locker, codeBlock, structure, slot.cachedOffset());
                    return RetryCacheLater;
                }
            }

            if (slot.slotBase() != base) {
                auto cacheStatus = prepareChainForCaching(globalObject, base, propertyName.uid(), slot);
                if (!cacheStatus)
                    return GiveUpOnCache;
                if (cacheStatus->flattenedDictionary)
                    return RetryCacheLater;

                if (cacheStatus->usesPolyProto) {
                    prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, base, propertyName, slot);
                    if (!prototypeAccessChain)
                        return GiveUpOnCache;
                    ASSERT(slot.isCacheableCustom() || prototypeAccessChain->slotBaseStructure(vm, structure)->get(vm, propertyName.uid()) == slot.cachedOffset());
                } else {
                    prototypeAccessChain = nullptr;
                    conditionSet = generateConditionsForPrototypePropertyHit(
                        vm, codeBlock, globalObject, structure, slot.slotBase(), ident.impl());
                    if (!conditionSet.isValid())
                        return GiveUpOnCache;
                    ASSERT(slot.isCacheableCustom() || conditionSet.slotBaseCondition().offset() == slot.cachedOffset());
                }
            }
        } else {
            if (!structure->propertyAccessesAreCacheable() || !structure->propertyAccessesAreCacheableForAbsence())
                return GiveUpOnCache;

            auto cacheStatus = prepareChainForCaching(globalObject, base, propertyName.uid(), nullptr);
            if (!cacheStatus)
                return GiveUpOnCache;

            if (cacheStatus->usesPolyProto) {
                prototypeAccessChain = PolyProtoAccessChain::tryCreate(globalObject, base, propertyName, slot);
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

        LOG_IC((ICEvent::InAddAccessCase, structure->classInfoForCells(), ident, slot.slotBase() == base));

        if (!newCase)
            newCase = AccessCase::create(vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, propertyName, wasFound ? slot.cachedOffset() : invalidOffset, structure, conditionSet, WTFMove(prototypeAccessChain));

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), propertyName, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfoForCells(), ident, slot.slotBase() == base));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchInBy(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier propertyName, bool wasFound, const PropertySlot& slot, StructureStubInfo& stubInfo, InByKind kind)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheInBy(globalObject, codeBlock, baseObject, propertyName, wasFound, slot, stubInfo, kind) == GiveUpOnCache) {
        LOG_IC((ICEvent::InReplaceWithGeneric, baseObject->classInfo(), Identifier::fromUid(globalObject->vm(), propertyName.uid())));
        repatchSlowPathCall(codeBlock, stubInfo, appropriateInByGaveUpFunction(kind));
    }
}

static InlineCacheAction tryCacheHasPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier brandID, bool wasFound, StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;

        Structure* structure = base->structure();

        InlineCacheAction action = actionForCell(vm, base);
        if (action != AttemptToCache)
            return action;

        bool isBaseProperty = true;
        LOG_IC((ICEvent::InAddAccessCase, structure->classInfoForCells(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::create(vm, codeBlock, wasFound ? AccessCase::InHit : AccessCase::InMiss, brandID, invalidOffset, structure, { }, { });

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InReplaceWithJump, structure->classInfoForCells(), ident, isBaseProperty));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchHasPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier brandID, bool wasFound, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheHasPrivateBrand(globalObject, codeBlock, baseObject, brandID, wasFound, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationHasPrivateBrandGaveUp);
}

static InlineCacheAction tryCacheCheckPrivateBrand(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, CacheableIdentifier brandID,
    StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
        if (forceICFailure(globalObject))
            return GiveUpOnCache;

        Structure* structure = base->structure();

        InlineCacheAction action = actionForCell(vm, base);
        if (action != AttemptToCache)
            return action;

        bool isBaseProperty = true;
        LOG_IC((ICEvent::CheckPrivateBrandAddAccessCase, structure->classInfoForCells(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::createCheckPrivateBrand(vm, codeBlock, brandID, structure);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::CheckPrivateBrandReplaceWithJump, structure->classInfoForCells(), ident, isBaseProperty));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);

    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchCheckPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, CacheableIdentifier brandID, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheCheckPrivateBrand(globalObject, codeBlock, baseObject, brandID, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationCheckPrivateBrandGaveUp);
}

static InlineCacheAction tryCacheSetPrivateBrand(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* base, Structure* oldStructure, CacheableIdentifier brandID,
    StructureStubInfo& stubInfo)
{
    VM& vm = globalObject->vm();
    AccessGenerationResult result;
    Identifier ident = Identifier::fromUid(vm, brandID.uid());

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
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
        LOG_IC((ICEvent::SetPrivateBrandAddAccessCase, oldStructure->classInfoForCells(), ident, isBaseProperty));

        Ref<AccessCase> newCase = AccessCase::createSetPrivateBrand(vm, codeBlock, brandID, oldStructure, newStructure);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), brandID, WTFMove(newCase));

        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::SetPrivateBrandReplaceWithJump, oldStructure->classInfoForCells(), ident, isBaseProperty));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchSetPrivateBrand(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSObject* baseObject, Structure* oldStructure, CacheableIdentifier brandID, StructureStubInfo& stubInfo)
{
    SuperSamplerScope superSamplerScope(false);

    if (tryCacheSetPrivateBrand(globalObject, codeBlock, baseObject, oldStructure,  brandID, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationSetPrivateBrandGaveUp);
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
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, vm);
        
        JSCell* value = valueValue.asCell();
        Structure* structure = value->structure();
        RefPtr<AccessCase> newCase;
        JSObject* prototype = jsDynamicCast<JSObject*>(prototypeValue);
        if (prototype) {
            if (!jsDynamicCast<JSObject*>(value)) {
                newCase = InstanceOfAccessCase::create(
                    vm, codeBlock, AccessCase::InstanceOfMiss, structure, ObjectPropertyConditionSet(),
                    prototype);
            } else if (structure->prototypeQueriesAreCacheable()) {
                // FIXME: Teach this to do poly proto.
                // https://bugs.webkit.org/show_bug.cgi?id=185663
                prepareChainForCaching(globalObject, value, nullptr, wasFound ? prototype : nullptr);
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
        
        LOG_IC((ICEvent::InstanceOfAddAccessCase, structure->classInfoForCells(), Identifier()));
        
        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, WTFMove(newCase));
        
        if (result.generatedSomeCode()) {
            LOG_IC((ICEvent::InstanceOfReplaceWithJump, structure->classInfoForCells(), Identifier()));
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
        }
    }
    
    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

static InlineCacheAction tryCacheArrayInByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue baseValue, JSValue index, StructureStubInfo& stubInfo)
{
    ASSERT(baseValue.isCell());

    if (!index.isInt32())
        return RetryCacheLater;

    VM& vm = globalObject->vm();
    AccessGenerationResult result;

    {
        GCSafeConcurrentJSLocker locker(codeBlock->m_lock, globalObject->vm());

        JSCell* base = baseValue.asCell();

        RefPtr<AccessCase> newCase;
        AccessCase::AccessType accessType = AccessCase::IndexedInt32InHit;
        if (base->type() == DirectArgumentsType)
            accessType = AccessCase::IndexedDirectArgumentsInHit;
        else if (base->type() == ScopedArgumentsType)
            accessType = AccessCase::IndexedScopedArgumentsInHit;
        else if (base->type() == StringType)
            accessType = AccessCase::IndexedStringInHit;
        else if (isTypedView(base->type())) {
            auto* typedArray = jsCast<JSArrayBufferView*>(base);
#if USE(JSVALUE32_64)
            if (typedArray->isResizableOrGrowableShared())
                return GiveUpOnCache;
#endif
            switch (typedArray->type()) {
            case Int8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt8InHit : AccessCase::IndexedTypedArrayInt8InHit;
                break;
            case Uint8ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8InHit : AccessCase::IndexedTypedArrayUint8InHit;
                break;
            case Uint8ClampedArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint8ClampedInHit : AccessCase::IndexedTypedArrayUint8ClampedInHit;
                break;
            case Int16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt16InHit : AccessCase::IndexedTypedArrayInt16InHit;
                break;
            case Uint16ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint16InHit : AccessCase::IndexedTypedArrayUint16InHit;
                break;
            case Int32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayInt32InHit : AccessCase::IndexedTypedArrayInt32InHit;
                break;
            case Uint32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayUint32InHit : AccessCase::IndexedTypedArrayUint32InHit;
                break;
            case Float32ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat32InHit : AccessCase::IndexedTypedArrayFloat32InHit;
                break;
            case Float64ArrayType:
                accessType = typedArray->isResizableOrGrowableShared() ? AccessCase::IndexedResizableTypedArrayFloat64InHit : AccessCase::IndexedTypedArrayFloat64InHit;
                break;
            // FIXME: Optimize BigInt64Array / BigUint64Array in IC
            // https://bugs.webkit.org/show_bug.cgi?id=221183
            case BigInt64ArrayType:
            case BigUint64ArrayType:
                return GiveUpOnCache;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        } else {
            IndexingType indexingShape = base->indexingType() & IndexingShapeMask;
            switch (indexingShape) {
            case Int32Shape:
                accessType = AccessCase::IndexedInt32InHit;
                break;
            case DoubleShape:
                ASSERT(Options::allowDoubleShape());
                accessType = AccessCase::IndexedDoubleInHit;
                break;
            case ContiguousShape:
                accessType = AccessCase::IndexedContiguousInHit;
                break;
            case ArrayStorageShape:
                accessType = AccessCase::IndexedArrayStorageInHit;
                break;
            case NoIndexingShape: {
                if (!base->isObject())
                    return GiveUpOnCache;

                if (base->structure()->mayInterceptIndexedAccesses() || base->structure()->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
                    return GiveUpOnCache;

                // FIXME: prepareChainForCaching is conservative. We should have another function which only cares about information related to this IC.
                auto cacheStatus = prepareChainForCaching(globalObject, base, nullptr, nullptr);
                if (!cacheStatus)
                    return GiveUpOnCache;

                if (cacheStatus->usesPolyProto)
                    return GiveUpOnCache;

                Structure* headStructure = base->structure();
                ObjectPropertyConditionSet conditionSet = generateConditionsForIndexedMiss(vm, codeBlock, globalObject, headStructure);
                if (!conditionSet.isValid())
                    return GiveUpOnCache;

                newCase = AccessCase::create(vm, codeBlock, AccessCase::IndexedNoIndexingInMiss, nullptr, invalidOffset, headStructure, conditionSet);
                break;
            }
            default:
                return GiveUpOnCache;
            }
        }

        if (!newCase)
            newCase = AccessCase::create(vm, codeBlock, accessType, nullptr);

        result = stubInfo.addAccessCase(locker, globalObject, codeBlock, ECMAMode::strict(), nullptr, newCase.releaseNonNull());

        if (result.generatedSomeCode())
            InlineAccess::rewireStubAsJumpInAccess(codeBlock, stubInfo, *result.handler());
    }

    fireWatchpointsAndClearStubIfNeeded(vm, stubInfo, codeBlock, result);
    return result.shouldGiveUpNow() ? GiveUpOnCache : RetryCacheLater;
}

void repatchArrayInByVal(JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue base, JSValue index, StructureStubInfo& stubInfo, InByKind kind)
{
    if (tryCacheArrayInByVal(globalObject, codeBlock, base, index, stubInfo) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, appropriateInByGaveUpFunction(kind));
}

void repatchInstanceOf(
    JSGlobalObject* globalObject, CodeBlock* codeBlock, JSValue valueValue, JSValue prototypeValue, StructureStubInfo& stubInfo,
    bool wasFound)
{
    SuperSamplerScope superSamplerScope(false);
    if (tryCacheInstanceOf(globalObject, codeBlock, valueValue, prototypeValue, stubInfo, wasFound) == GiveUpOnCache)
        repatchSlowPathCall(codeBlock, stubInfo, operationInstanceOfGaveUp);
}

void linkDirectCall(DirectCallLinkInfo& callLinkInfo, CodeBlock* calleeCodeBlock, CodePtr<JSEntryPtrTag> codePtr)
{
    // DirectCall is only used from DFG / FTL.
    callLinkInfo.setCallTarget(jsCast<FunctionCodeBlock*>(calleeCodeBlock), CodeLocationLabel<JSEntryPtrTag>(codePtr));
    if (calleeCodeBlock)
        calleeCodeBlock->linkIncomingCall(callLinkInfo.owner(), &callLinkInfo);
}

void linkPolymorphicCall(VM& vm, JSCell* owner, CallFrame* callFrame, CallLinkInfo& callLinkInfo, CallVariant newVariant)
{
    RELEASE_ASSERT(callLinkInfo.allowStubs());

    // During execution of linkPolymorphicCall, we strongly assume that we never do GC.
    // GC jettisons CodeBlocks, changes CallLinkInfo etc. and breaks assumption done before and after this call.
    DeferGCForAWhile deferGCForAWhile(vm);
    
    if (!newVariant) {
        callLinkInfo.setVirtualCall(vm);
        return;
    }

    CodeBlock* callerCodeBlock = jsDynamicCast<CodeBlock*>(owner); // WebAssembly -> JS stubs don't have a valid CodeBlock.
    ASSERT(owner);
#if ENABLE(WEBASSEMBLY)
    bool isWebAssembly = owner->inherits<JSWebAssemblyModule>();
#else
    bool isWebAssembly = false;
#endif
    bool isDataIC = callLinkInfo.isDataIC();
    bool isTailCall = callLinkInfo.isTailCall();

    bool isClosureCall = false;
    CallVariantList list;
    if (PolymorphicCallStubRoutine* stub = callLinkInfo.stub()) {
        list = stub->variants();
        isClosureCall = stub->isClosureCall();
    } else if (JSObject* oldCallee = callLinkInfo.callee())
        list = CallVariantList { CallVariant(oldCallee) };
    
    list = variantListWithVariant(list, newVariant);

    // If there are any closure calls then it makes sense to treat all of them as closure calls.
    // This makes switching on callee cheaper. It also produces profiling that's easier on the DFG;
    // the DFG doesn't really want to deal with a combination of closure and non-closure callees.
    if (!isClosureCall) {
        for (CallVariant variant : list)  {
            if (variant.isClosureCall()) {
                list = despecifiedVariantList(list);
                isClosureCall = true;
                break;
            }
        }
    }

    if (isClosureCall)
        callLinkInfo.setHasSeenClosure();

    // If we are over the limit, just use a normal virtual call.
    unsigned maxPolymorphicCallVariantListSize;
    if (isWebAssembly)
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForWebAssemblyToJS();
    else if (callerCodeBlock->jitType() == JITCode::topTierJIT())
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSizeForTopTier();
    else
        maxPolymorphicCallVariantListSize = Options::maxPolymorphicCallVariantListSize();

    // We use list.size() instead of callSlots.size() because we respect CallVariant size for now.
    if (list.size() > maxPolymorphicCallVariantListSize) {
        callLinkInfo.setVirtualCall(vm);
        return;
    }

    Vector<CallSlot, 16> callSlots;
    
    // Figure out what our cases are.
    for (CallVariant variant : list) {
        CodeBlock* codeBlock = nullptr;
        if (variant.executable() && !variant.executable()->isHostFunction()) {
            ExecutableBase* executable = variant.executable();
            codeBlock = jsCast<FunctionExecutable*>(executable)->codeBlockForCall();
            // If we cannot handle a callee, because we don't have a CodeBlock,
            // assume that it's better for this whole thing to be a virtual call.
            if (!codeBlock) {
                callLinkInfo.setVirtualCall(vm);
                return;
            }
        }

        JSCell* caseValue = nullptr;
        if (isClosureCall) {
            caseValue = variant.executable();
            // FIXME: We could add a fast path for InternalFunction with closure call.
            // https://bugs.webkit.org/show_bug.cgi?id=179311
            if (!caseValue)
                continue;
        } else {
            if (auto* function = variant.function())
                caseValue = function;
            else
                caseValue = variant.internalFunction();
        }

        CallSlot slot;

        CodePtr<JSEntryPtrTag> codePtr;
        if (variant.executable()) {
            ASSERT(variant.executable()->hasJITCodeForCall());

            codePtr = jsToWasmICCodePtr(callLinkInfo.specializationKind(), variant.function());
            if (!codePtr) {
                ArityCheckMode arityCheck = ArityCheckNotRequired;
                if (codeBlock) {
                    ASSERT(!variant.executable()->isHostFunction());
                    if ((callFrame->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo.isVarargs()))
                        arityCheck = MustCheckArity;

                }
                codePtr = variant.executable()->generatedJITCodeForCall()->addressForCall(arityCheck);
                slot.m_arityCheckMode = arityCheck;
            }
        } else {
            ASSERT(variant.internalFunction());
            codePtr = vm.getCTIInternalFunctionTrampolineFor(CodeForCall);
        }

        slot.m_index = callSlots.size();
        slot.m_target = codePtr;
        slot.m_codeBlock = codeBlock;
        slot.m_calleeOrExecutable = caseValue;

        callSlots.append(WTFMove(slot));
    }

    bool notUsingCounting = isWebAssembly || callerCodeBlock->jitType() == JITCode::topTierJIT();
    if (callSlots.isEmpty())
        notUsingCounting = true;

    CallFrame* callerFrame = nullptr;
    if (!isTailCall)
        callerFrame = callFrame->callerFrame();

    if (isDataIC) {
        CommonJITThunkID jitThunk = CommonJITThunkID::PolymorphicThunkForRegularCall;
        if (isClosureCall)
            jitThunk = isTailCall ? CommonJITThunkID::PolymorphicThunkForTailCallForClosure : CommonJITThunkID::PolymorphicThunkForRegularCallForClosure;
        else
            jitThunk = isTailCall ? CommonJITThunkID::PolymorphicThunkForTailCall : CommonJITThunkID::PolymorphicThunkForRegularCall;

        auto stubRoutine = PolymorphicCallStubRoutine::create(vm.getCTIStub(jitThunk).retagged<JITStubRoutinePtrTag>(), vm, owner, callerFrame, callLinkInfo, callSlots, nullptr, notUsingCounting, isClosureCall);

        // If there had been a previous stub routine, that one will die as soon as the GC runs and sees
        // that it's no longer on stack.
        callLinkInfo.setStub(WTFMove(stubRoutine));

        // The call link info no longer has a call cache apart from the jump to the polymorphic call
        // stub.
        if (callLinkInfo.isOnList())
            callLinkInfo.remove();

        return;
    }

    ASSERT(callLinkInfo.type() == CallLinkInfo::Type::Optimizing);

    CCallHelpers jit(callerCodeBlock);
    GPRReg calleeGPR = BaselineJITRegisters::Call::calleeGPR;

    UniqueArray<uint32_t> fastCounts;

    if (!notUsingCounting) {
        fastCounts = makeUniqueArray<uint32_t>(callSlots.size());
        memset(fastCounts.get(), 0, callSlots.size() * sizeof(uint32_t));
    }

    Vector<int64_t> caseValues;
    caseValues.reserveInitialCapacity(callSlots.size());
    for (auto& slot : callSlots) {
        int64_t caseValue = bitwise_cast<intptr_t>(slot.m_calleeOrExecutable);
#if ASSERT_ENABLED
        if (caseValues.contains(caseValue)) {
            dataLog("ERROR: Attempt to add duplicate case value.\n");
            dataLog("Existing case values: ");
            CommaPrinter comma;
            for (auto& value : caseValues)
                dataLog(comma, value);
            dataLog("\n");
            dataLog("Attempting to add: ", caseValue, "\n");
            dataLog("Variant list: ", listDump(callSlots.map([&](auto& slot) {
                return PolymorphicCallCase(CallVariant(slot.m_calleeOrExecutable), slot.m_codeBlock);
            })), "\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
#endif
        caseValues.append(caseValue);
    }

    GPRReg comparisonValueGPR = calleeGPR;
    if (isClosureCall)
        comparisonValueGPR = AssemblyHelpers::selectScratchGPR(calleeGPR);

    GPRReg fastCountsBaseGPR = AssemblyHelpers::selectScratchGPR(calleeGPR, comparisonValueGPR);
    jit.move(CCallHelpers::TrustedImmPtr(fastCounts.get()), fastCountsBaseGPR);

    CCallHelpers::JumpList slowPath;
    if (isClosureCall) {
        // Verify that we have a function and stash the executable in scratchGPR.
#if USE(JSVALUE64)
        if (isTailCall)
            slowPath.append(jit.branchIfNotCell(calleeGPR, DoNotHaveTagRegisters));
        else
            slowPath.append(jit.branchIfNotCell(calleeGPR));
#else
        // We would have already checked that the callee is a cell.
#endif
        // FIXME: We could add a fast path for InternalFunction with closure call.
        slowPath.append(jit.branchIfNotFunction(calleeGPR));

        jit.loadPtr(CCallHelpers::Address(calleeGPR, JSFunction::offsetOfExecutableOrRareData()), comparisonValueGPR);
        auto hasExecutable = jit.branchTestPtr(CCallHelpers::Zero, comparisonValueGPR, CCallHelpers::TrustedImm32(JSFunction::rareDataTag));
        jit.loadPtr(CCallHelpers::Address(comparisonValueGPR, FunctionRareData::offsetOfExecutable() - JSFunction::rareDataTag), comparisonValueGPR);
        hasExecutable.link(&jit);
    }

    BinarySwitch binarySwitch(comparisonValueGPR, caseValues, BinarySwitch::IntPtr);
    while (binarySwitch.advance(jit)) {
        size_t caseIndex = binarySwitch.caseIndex();
        auto& slot = callSlots[caseIndex];
        CallVariant variant(slot.m_calleeOrExecutable);
        CodeBlock* codeBlock = slot.m_codeBlock;
        CodePtr<JSEntryPtrTag> codePtr = slot.m_target;
        if (fastCounts) {
            jit.add32(
                CCallHelpers::TrustedImm32(1),
                CCallHelpers::Address(fastCountsBaseGPR, caseIndex * sizeof(uint32_t)));
        }
        if (isTailCall) {
            if (codeBlock)
                jit.storePtr(CCallHelpers::TrustedImmPtr(codeBlock), CCallHelpers::calleeFrameCodeBlockBeforeTailCall());
            jit.nearTailCallThunk(CodeLocationLabel { codePtr });
        } else {
            if (codeBlock)
                jit.storePtr(CCallHelpers::TrustedImmPtr(codeBlock), CCallHelpers::calleeFrameCodeBlockBeforeCall());
            jit.nearCallThunk(CodeLocationLabel { codePtr });
            jit.jumpThunk(callLinkInfo.doneLocation());
        }
    }

    slowPath.link(&jit);
    binarySwitch.fallThrough().link(&jit);
    jit.move(CCallHelpers::TrustedImmPtr(&callLinkInfo), GPRInfo::regT2);
    if (isTailCall)
        jit.nearTailCallThunk(CodeLocationLabel { vm.getCTIStub(CommonJITThunkID::PolymorphicRepatchThunk).code() });
    else {
        jit.nearCallThunk(CodeLocationLabel { vm.getCTIStub(CommonJITThunkID::PolymorphicRepatchThunk).code() });
        jit.jumpThunk(callLinkInfo.doneLocation());
    }

    LinkBuffer patchBuffer(jit, owner, LinkBuffer::Profile::InlineCache, JITCompilationCanFail);
    if (patchBuffer.didFailToAllocate()) {
        callLinkInfo.setVirtualCall(vm);
        return;
    }
    
    auto stubRoutine = PolymorphicCallStubRoutine::create(
        FINALIZE_CODE_FOR(
            callerCodeBlock, patchBuffer, JITStubRoutinePtrTag,
            "Polymorphic call stub for %s, return point %p, targets %s",
                isWebAssembly ? "WebAssembly" : toCString(*callerCodeBlock).data(), callLinkInfo.doneLocation().taggedPtr(),
                toCString(listDump(callSlots.map([&](auto& slot) { return PolymorphicCallCase(CallVariant(slot.m_calleeOrExecutable), slot.m_codeBlock); }))).data()),
        vm, owner, callerFrame, callLinkInfo, callSlots, WTFMove(fastCounts), notUsingCounting, isClosureCall);

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
    repatchSlowPathCall(codeBlock, stubInfo, appropriateGetByOptimizeFunction(kind));
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetPutBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, PutByKind kind)
{
    CodePtr<CFunctionPtrTag> optimizedFunction;
    switch (kind) {
    case PutByKind::ByIdStrict:
        optimizedFunction = operationPutByIdStrictOptimize;
        break;
    case PutByKind::ByIdSloppy:
        optimizedFunction = operationPutByIdSloppyOptimize;
        break;
    case PutByKind::ByIdDirectStrict:
        optimizedFunction = operationPutByIdDirectStrictOptimize;
        break;
    case PutByKind::ByIdDirectSloppy:
        optimizedFunction = operationPutByIdDirectSloppyOptimize;
        break;
    case PutByKind::DefinePrivateNameById:
        optimizedFunction = operationPutByIdDefinePrivateFieldStrictOptimize;
        break;
    case PutByKind::SetPrivateNameById:
        optimizedFunction = operationPutByIdSetPrivateFieldStrictOptimize;
        break;
    case PutByKind::ByValStrict:
        optimizedFunction = operationPutByValStrictOptimize;
        break;
    case PutByKind::ByValSloppy:
        optimizedFunction = operationPutByValSloppyOptimize;
        break;
    case PutByKind::ByValDirectStrict:
        optimizedFunction = operationDirectPutByValStrictOptimize;
        break;
    case PutByKind::ByValDirectSloppy:
        optimizedFunction = operationDirectPutByValSloppyOptimize;
        break;
    case PutByKind::DefinePrivateNameByVal:
        optimizedFunction = operationPutByValDefinePrivateFieldOptimize;
        break;
    case PutByKind::SetPrivateNameByVal:
        optimizedFunction = operationPutByValSetPrivateFieldOptimize;
        break;
    }

    repatchSlowPathCall(codeBlock, stubInfo, optimizedFunction);
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetDelBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, DelByKind kind)
{
    switch (kind) {
    case DelByKind::ByIdStrict:
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdStrictOptimize);
        break;
    case DelByKind::ByIdSloppy:
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByIdSloppyOptimize);
        break;
    case DelByKind::ByValStrict:
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValStrictOptimize);
        break;
    case DelByKind::ByValSloppy:
        repatchSlowPathCall(codeBlock, stubInfo, operationDeleteByValSloppyOptimize);
        break;
    }
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetInBy(CodeBlock* codeBlock, StructureStubInfo& stubInfo, InByKind kind)
{
    repatchSlowPathCall(codeBlock, stubInfo, appropriateInByOptimizeFunction(kind));
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetHasPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationHasPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetInstanceOf(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationInstanceOfOptimize);
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetCheckPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationCheckPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

void resetSetPrivateBrand(CodeBlock* codeBlock, StructureStubInfo& stubInfo)
{
    repatchSlowPathCall(codeBlock, stubInfo, operationSetPrivateBrandOptimize);
    InlineAccess::resetStubAsJumpInAccess(codeBlock, stubInfo);
}

#endif

} // namespace JSC
