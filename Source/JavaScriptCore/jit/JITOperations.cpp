/*
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "JITOperations.h"

#if ENABLE(JIT)

#include "ArrayConstructor.h"
#include "CommonSlowPaths.h"
#include "DFGCompilationMode.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGThunks.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "DirectArguments.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "ExceptionFuzz.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "JIT.h"
#include "JITExceptions.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JSCInlines.h"
#include "JSGeneratorFunction.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLexicalEnvironment.h"
#include "JSPropertyNameEnumerator.h"
#include "JSStackInlines.h"
#include "JSWithScope.h"
#include "LegacyProfiler.h"
#include "ObjectConstructor.h"
#include "PropertyName.h"
#include "Repatch.h"
#include "ScopedArguments.h"
#include "SuperSampler.h"
#include "TestRunnerUtils.h"
#include "TypeProfilerLog.h"
#include "VMInlines.h"
#include <wtf/InlineASM.h>

namespace JSC {

extern "C" {

#if COMPILER(MSVC)
void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

#define OUR_RETURN_ADDRESS _ReturnAddress()
#else
#define OUR_RETURN_ADDRESS __builtin_return_address(0)
#endif

#if ENABLE(OPCODE_SAMPLING)
#define CTI_SAMPLER vm->interpreter->sampler()
#else
#define CTI_SAMPLER 0
#endif


void JIT_OPERATION operationThrowStackOverflowError(ExecState* exec, CodeBlock* codeBlock)
{
    // We pass in our own code block, because the callframe hasn't been populated.
    VM* vm = codeBlock->vm();

    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);
    if (!callerFrame)
        callerFrame = exec;

    NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
    throwStackOverflowError(callerFrame);
}

#if ENABLE(WEBASSEMBLY)
void JIT_OPERATION operationThrowDivideError(ExecState* exec)
{
    VM* vm = &exec->vm();
    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);

    NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
    ErrorHandlingScope errorScope(*vm);
    vm->throwException(callerFrame, createError(callerFrame, ASCIILiteral("Division by zero or division overflow.")));
}

void JIT_OPERATION operationThrowOutOfBoundsAccessError(ExecState* exec)
{
    VM* vm = &exec->vm();
    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);

    NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
    ErrorHandlingScope errorScope(*vm);
    vm->throwException(callerFrame, createError(callerFrame, ASCIILiteral("Out-of-bounds access.")));
}
#endif

int32_t JIT_OPERATION operationCallArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForCall);
    if (missingArgCount < 0) {
        VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
        CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);
        NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
        throwStackOverflowError(callerFrame);
    }

    return missingArgCount;
}

int32_t JIT_OPERATION operationConstructArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForConstruct);
    if (missingArgCount < 0) {
        VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
        CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);
        NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
        throwStackOverflowError(callerFrame);
    }

    return missingArgCount;
}

EncodedJSValue JIT_OPERATION operationGetById(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue base, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    Identifier ident = Identifier::fromUid(vm, uid);
    return JSValue::encode(baseValue.get(exec, ident, slot));
}

EncodedJSValue JIT_OPERATION operationGetByIdGeneric(ExecState* exec, EncodedJSValue base, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    Identifier ident = Identifier::fromUid(vm, uid);
    return JSValue::encode(baseValue.get(exec, ident, slot));
}

EncodedJSValue JIT_OPERATION operationGetByIdOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue base, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    Identifier ident = Identifier::fromUid(vm, uid);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::Get);
    
    bool hasResult = baseValue.getPropertySlot(exec, ident, slot);
    if (stubInfo->considerCaching())
        repatchGetByID(exec, baseValue, ident, slot, *stubInfo);
    
    return JSValue::encode(hasResult? slot.getValue(exec, ident) : jsUndefined());
}

EncodedJSValue JIT_OPERATION operationInOptimize(ExecState* exec, StructureStubInfo* stubInfo, JSCell* base, UniquedStringImpl* key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (!base->isObject()) {
        vm->throwException(exec, createInvalidInParameterError(exec, base));
        return JSValue::encode(jsUndefined());
    }
    
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    Identifier ident = Identifier::fromUid(vm, key);
    PropertySlot slot(base, PropertySlot::InternalMethodType::HasProperty);
    bool result = asObject(base)->getPropertySlot(exec, ident, slot);
    
    RELEASE_ASSERT(accessType == stubInfo->accessType);
    
    if (stubInfo->considerCaching())
        repatchIn(exec, base, ident, result, slot, *stubInfo);
    
    return JSValue::encode(jsBoolean(result));
}

EncodedJSValue JIT_OPERATION operationIn(ExecState* exec, StructureStubInfo* stubInfo, JSCell* base, UniquedStringImpl* key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;

    if (!base->isObject()) {
        vm->throwException(exec, createInvalidInParameterError(exec, base));
        return JSValue::encode(jsUndefined());
    }

    Identifier ident = Identifier::fromUid(vm, key);
    return JSValue::encode(jsBoolean(asObject(base)->hasProperty(exec, ident)));
}

EncodedJSValue JIT_OPERATION operationGenericIn(ExecState* exec, JSCell* base, EncodedJSValue key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(jsBoolean(CommonSlowPaths::opIn(exec, JSValue::decode(key), base)));
}

void JIT_OPERATION operationPutByIdStrict(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;
    
    Identifier ident = Identifier::fromUid(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), true, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).putInline(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdNonStrict(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;
    
    Identifier ident = Identifier::fromUid(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), false, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).putInline(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectStrict(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;
    
    Identifier ident = Identifier::fromUid(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), true, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectNonStrict(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    stubInfo->tookSlowPath = true;
    
    Identifier ident = Identifier::fromUid(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), false, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident = Identifier::fromUid(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, true, exec->codeBlock()->putByIdContext());

    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr;
    baseValue.putInline(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCaching())
        repatchPutByID(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
}

void JIT_OPERATION operationPutByIdNonStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident = Identifier::fromUid(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, false, exec->codeBlock()->putByIdContext());

    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr;    
    baseValue.putInline(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCaching())
        repatchPutByID(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
}

void JIT_OPERATION operationPutByIdDirectStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident = Identifier::fromUid(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, true, exec->codeBlock()->putByIdContext());
    
    Structure* structure = baseObject->structure(*vm);
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCaching())
        repatchPutByID(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
}

void JIT_OPERATION operationPutByIdDirectNonStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, UniquedStringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident = Identifier::fromUid(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, false, exec->codeBlock()->putByIdContext());
    
    Structure* structure = baseObject->structure(*vm);
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->considerCaching())
        repatchPutByID(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
}

void JIT_OPERATION operationReallocateStorageAndFinishPut(ExecState* exec, JSObject* base, Structure* structure, PropertyOffset offset, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(structure->outOfLineCapacity() > base->structure(vm)->outOfLineCapacity());
    ASSERT(!vm.heap.storageAllocator().fastPathShouldSucceed(structure->outOfLineCapacity() * sizeof(JSValue)));
    base->setStructureAndReallocateStorageIfNecessary(vm, structure);
    base->putDirect(vm, offset, JSValue::decode(value));
}

ALWAYS_INLINE static bool isStringOrSymbol(JSValue value)
{
    return value.isString() || value.isSymbol();
}

static void putByVal(CallFrame* callFrame, JSValue baseValue, JSValue subscript, JSValue value, ByValInfo* byValInfo)
{
    VM& vm = callFrame->vm();
    if (LIKELY(subscript.isUInt32())) {
        byValInfo->tookSlowPath = true;
        uint32_t i = subscript.asUInt32();
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i))
                object->setIndexQuickly(callFrame->vm(), i, value);
            else {
                // FIXME: This will make us think that in-bounds typed array accesses are actually
                // out-of-bounds.
                // https://bugs.webkit.org/show_bug.cgi?id=149886
                byValInfo->arrayProfile->setOutOfBounds();
                object->methodTable(vm)->putByIndex(object, callFrame, i, value, callFrame->codeBlock()->isStrictMode());
            }
        } else
            baseValue.putByIndex(callFrame, i, value, callFrame->codeBlock()->isStrictMode());
        return;
    }

    auto property = subscript.toPropertyKey(callFrame);
    // Don't put to an object if toString threw an exception.
    if (callFrame->vm().exception())
        return;

    if (byValInfo->stubInfo && (!isStringOrSymbol(subscript) || byValInfo->cachedId != property))
        byValInfo->tookSlowPath = true;

    PutPropertySlot slot(baseValue, callFrame->codeBlock()->isStrictMode());
    baseValue.putInline(callFrame, property, value, slot);
}

static void directPutByVal(CallFrame* callFrame, JSObject* baseObject, JSValue subscript, JSValue value, ByValInfo* byValInfo)
{
    bool isStrictMode = callFrame->codeBlock()->isStrictMode();
    if (LIKELY(subscript.isUInt32())) {
        // Despite its name, JSValue::isUInt32 will return true only for positive boxed int32_t; all those values are valid array indices.
        byValInfo->tookSlowPath = true;
        uint32_t index = subscript.asUInt32();
        ASSERT(isIndex(index));
        if (baseObject->canSetIndexQuicklyForPutDirect(index)) {
            baseObject->setIndexQuickly(callFrame->vm(), index, value);
            return;
        }

        // FIXME: This will make us think that in-bounds typed array accesses are actually
        // out-of-bounds.
        // https://bugs.webkit.org/show_bug.cgi?id=149886
        byValInfo->arrayProfile->setOutOfBounds();
        baseObject->putDirectIndex(callFrame, index, value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    if (subscript.isDouble()) {
        double subscriptAsDouble = subscript.asDouble();
        uint32_t subscriptAsUInt32 = static_cast<uint32_t>(subscriptAsDouble);
        if (subscriptAsDouble == subscriptAsUInt32 && isIndex(subscriptAsUInt32)) {
            byValInfo->tookSlowPath = true;
            baseObject->putDirectIndex(callFrame, subscriptAsUInt32, value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
            return;
        }
    }

    // Don't put to an object if toString threw an exception.
    auto property = subscript.toPropertyKey(callFrame);
    if (callFrame->vm().exception())
        return;

    if (Optional<uint32_t> index = parseIndex(property)) {
        byValInfo->tookSlowPath = true;
        baseObject->putDirectIndex(callFrame, index.value(), value, 0, isStrictMode ? PutDirectIndexShouldThrow : PutDirectIndexShouldNotThrow);
        return;
    }

    if (byValInfo->stubInfo && (!isStringOrSymbol(subscript) || byValInfo->cachedId != property))
        byValInfo->tookSlowPath = true;

    PutPropertySlot slot(baseObject, isStrictMode);
    baseObject->putDirect(callFrame->vm(), property, value, slot);
}

enum class OptimizationResult {
    NotOptimized,
    SeenOnce,
    Optimized,
    GiveUp,
};

static OptimizationResult tryPutByValOptimize(ExecState* exec, JSValue baseValue, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    // See if it's worth optimizing at all.
    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    VM& vm = exec->vm();

    if (baseValue.isObject() && subscript.isInt32()) {
        JSObject* object = asObject(baseValue);

        ASSERT(exec->bytecodeOffset());
        ASSERT(!byValInfo->stubRoutine);

        Structure* structure = object->structure(vm);
        if (hasOptimizableIndexing(structure)) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(structure);
            if (jitArrayModePermitsPut(arrayMode) && arrayMode != byValInfo->arrayMode) {
                CodeBlock* codeBlock = exec->codeBlock();
                ConcurrentJITLocker locker(codeBlock->m_lock);
                byValInfo->arrayProfile->computeUpdatedPrediction(locker, codeBlock, structure);

                JIT::compilePutByVal(&vm, exec->codeBlock(), byValInfo, returnAddress, arrayMode);
                optimizationResult = OptimizationResult::Optimized;
            }
        }

        // If we failed to patch and we have some object that intercepts indexed get, then don't even wait until 10 times.
        if (optimizationResult != OptimizationResult::Optimized && object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            optimizationResult = OptimizationResult::GiveUp;
    }

    if (baseValue.isObject() && isStringOrSymbol(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(exec);
        if (!subscript.isString() || !parseIndex(propertyName)) {
            ASSERT(exec->bytecodeOffset());
            ASSERT(!byValInfo->stubRoutine);
            if (byValInfo->seen) {
                if (byValInfo->cachedId == propertyName) {
                    JIT::compilePutByValWithCachedId(&vm, exec->codeBlock(), byValInfo, returnAddress, NotDirect, propertyName);
                    optimizationResult = OptimizationResult::Optimized;
                } else {
                    // Seem like a generic property access site.
                    optimizationResult = OptimizationResult::GiveUp;
                }
            } else {
                byValInfo->seen = true;
                byValInfo->cachedId = propertyName;
                optimizationResult = OptimizationResult::SeenOnce;
            }
        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. For cases where we see non-index-intercepting
        // objects, this gives 10 iterations worth of opportunity for us to observe
        // that the put_by_val may be polymorphic. We count up slowPathCount even if
        // the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    return optimizationResult;
}

void JIT_OPERATION operationPutByValOptimize(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    if (tryPutByValOptimize(exec, baseValue, subscript, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS)) == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationPutByValGeneric));
    }
    putByVal(exec, baseValue, subscript, value, byValInfo);
}

static OptimizationResult tryDirectPutByValOptimize(ExecState* exec, JSObject* object, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    // See if it's worth optimizing at all.
    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    VM& vm = exec->vm();

    if (subscript.isInt32()) {
        ASSERT(exec->bytecodeOffset());
        ASSERT(!byValInfo->stubRoutine);

        Structure* structure = object->structure(vm);
        if (hasOptimizableIndexing(structure)) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(structure);
            if (jitArrayModePermitsPut(arrayMode) && arrayMode != byValInfo->arrayMode) {
                CodeBlock* codeBlock = exec->codeBlock();
                ConcurrentJITLocker locker(codeBlock->m_lock);
                byValInfo->arrayProfile->computeUpdatedPrediction(locker, codeBlock, structure);

                JIT::compileDirectPutByVal(&vm, exec->codeBlock(), byValInfo, returnAddress, arrayMode);
                optimizationResult = OptimizationResult::Optimized;
            }
        }

        // If we failed to patch and we have some object that intercepts indexed get, then don't even wait until 10 times.
        if (optimizationResult != OptimizationResult::Optimized && object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            optimizationResult = OptimizationResult::GiveUp;
    } else if (isStringOrSymbol(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(exec);
        Optional<uint32_t> index = parseIndex(propertyName);

        if (!subscript.isString() || !index) {
            ASSERT(exec->bytecodeOffset());
            ASSERT(!byValInfo->stubRoutine);
            if (byValInfo->seen) {
                if (byValInfo->cachedId == propertyName) {
                    JIT::compilePutByValWithCachedId(&vm, exec->codeBlock(), byValInfo, returnAddress, Direct, propertyName);
                    optimizationResult = OptimizationResult::Optimized;
                } else {
                    // Seem like a generic property access site.
                    optimizationResult = OptimizationResult::GiveUp;
                }
            } else {
                byValInfo->seen = true;
                byValInfo->cachedId = propertyName;
                optimizationResult = OptimizationResult::SeenOnce;
            }
        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. For cases where we see non-index-intercepting
        // objects, this gives 10 iterations worth of opportunity for us to observe
        // that the get_by_val may be polymorphic. We count up slowPathCount even if
        // the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    return optimizationResult;
}

void JIT_OPERATION operationDirectPutByValOptimize(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    JSObject* object = asObject(baseValue);
    if (tryDirectPutByValOptimize(exec, object, subscript, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS)) == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationDirectPutByValGeneric));
    }

    directPutByVal(exec, object, subscript, value, byValInfo);
}

void JIT_OPERATION operationPutByValGeneric(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(exec, baseValue, subscript, value, byValInfo);
}


void JIT_OPERATION operationDirectPutByValGeneric(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    directPutByVal(exec, asObject(baseValue), subscript, value, byValInfo);
}

EncodedJSValue JIT_OPERATION operationCallEval(ExecState* exec, ExecState* execCallee)
{
    UNUSED_PARAM(exec);

    execCallee->setCodeBlock(0);

    if (!isHostFunction(execCallee->calleeAsValue(), globalFuncEval))
        return JSValue::encode(JSValue());

    VM* vm = &execCallee->vm();
    JSValue result = eval(execCallee);
    if (vm->exception())
        return EncodedJSValue();
    
    return JSValue::encode(result);
}

static SlowPathReturnType handleHostCall(ExecState* execCallee, JSValue callee, CallLinkInfo* callLinkInfo)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();

    execCallee->setCodeBlock(0);

    if (callLinkInfo->specializationKind() == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(callee, callData);
    
        ASSERT(callType != CallType::JS);
    
        if (callType == CallType::Host) {
            NativeCallFrameTracer tracer(vm, execCallee);
            execCallee->setCallee(asObject(callee));
            vm->hostCallReturnValue = JSValue::decode(callData.native.function(execCallee));
            if (vm->exception()) {
                return encodeResult(
                    vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }

            return encodeResult(
                bitwise_cast<void*>(getHostCallReturnValue),
                reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
        }
    
        ASSERT(callType == CallType::None);
        exec->vm().throwException(exec, createNotAFunctionError(exec, callee));
        return encodeResult(
            vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
            reinterpret_cast<void*>(KeepTheFrame));
    }

    ASSERT(callLinkInfo->specializationKind() == CodeForConstruct);
    
    ConstructData constructData;
    ConstructType constructType = getConstructData(callee, constructData);
    
    ASSERT(constructType != ConstructType::JS);
    
    if (constructType == ConstructType::Host) {
        NativeCallFrameTracer tracer(vm, execCallee);
        execCallee->setCallee(asObject(callee));
        vm->hostCallReturnValue = JSValue::decode(constructData.native.function(execCallee));
        if (vm->exception()) {
            return encodeResult(
                vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }

        return encodeResult(bitwise_cast<void*>(getHostCallReturnValue), reinterpret_cast<void*>(KeepTheFrame));
    }
    
    ASSERT(constructType == ConstructType::None);
    exec->vm().throwException(exec, createNotAConstructorError(exec, callee));
    return encodeResult(
        vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
        reinterpret_cast<void*>(KeepTheFrame));
}

SlowPathReturnType JIT_OPERATION operationLinkCall(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell) {
        // FIXME: We should cache these kinds of calls. They can be common and currently they are
        // expensive.
        // https://bugs.webkit.org/show_bug.cgi?id=144458
        return handleHostCall(execCallee, calleeAsValue, callLinkInfo);
    }

    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction()) {
        codePtr = executable->entrypointFor(kind, MustCheckArity);
#if ENABLE(WEBASSEMBLY)
    } else if (executable->isWebAssemblyExecutable()) {
        WebAssemblyExecutable* webAssemblyExecutable = static_cast<WebAssemblyExecutable*>(executable);
        webAssemblyExecutable->prepareForExecution(execCallee);
        codeBlock = webAssemblyExecutable->codeBlockForCall();
        ASSERT(codeBlock);
        ArityCheckMode arity;
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()))
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = webAssemblyExecutable->entrypointFor(kind, arity);
#endif
    } else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

        if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
            exec->vm().throwException(exec, createNotAConstructorError(exec, callee));
            return encodeResult(
                vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }

        JSObject* error = functionExecutable->prepareForExecution(execCallee, callee, scope, kind);
        if (error) {
            exec->vm().throwException(exec, error);
            return encodeResult(
                vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                reinterpret_cast<void*>(KeepTheFrame));
        }
        codeBlock = functionExecutable->codeBlockFor(kind);
        ArityCheckMode arity;
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo->isVarargs())
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(kind, arity);
    }
    if (!callLinkInfo->seenOnce())
        callLinkInfo->setSeen();
    else
        linkFor(execCallee, *callLinkInfo, codeBlock, callee, codePtr);
    
    return encodeResult(codePtr.executableAddress(), reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

inline SlowPathReturnType virtualForWithFunction(
    ExecState* execCallee, CallLinkInfo* callLinkInfo, JSCell*& calleeAsFunctionCell)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    CodeSpecializationKind kind = callLinkInfo->specializationKind();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue calleeAsValue = execCallee->calleeAsValue();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell))
        return handleHostCall(execCallee, calleeAsValue, callLinkInfo);
    
    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = function->scopeUnchecked();
    ExecutableBase* executable = function->executable();
    if (UNLIKELY(!executable->hasJITCodeFor(kind))) {
        bool isWebAssemblyExecutable = false;
#if ENABLE(WEBASSEMBLY)
        isWebAssemblyExecutable = executable->isWebAssemblyExecutable();
#endif
        if (!isWebAssemblyExecutable) {
            FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);

            if (!isCall(kind) && functionExecutable->constructAbility() == ConstructAbility::CannotConstruct) {
                exec->vm().throwException(exec, createNotAConstructorError(exec, function));
                return encodeResult(
                    vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }

            JSObject* error = functionExecutable->prepareForExecution(execCallee, function, scope, kind);
            if (error) {
                exec->vm().throwException(exec, error);
                return encodeResult(
                    vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }
        } else {
#if ENABLE(WEBASSEMBLY)
            if (!isCall(kind)) {
                exec->vm().throwException(exec, createNotAConstructorError(exec, function));
                return encodeResult(
                    vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress(),
                    reinterpret_cast<void*>(KeepTheFrame));
            }

            WebAssemblyExecutable* webAssemblyExecutable = static_cast<WebAssemblyExecutable*>(executable);
            webAssemblyExecutable->prepareForExecution(execCallee);
#endif
        }
    }
    return encodeResult(executable->entrypointFor(
        kind, MustCheckArity).executableAddress(),
        reinterpret_cast<void*>(callLinkInfo->callMode() == CallMode::Tail ? ReuseTheFrame : KeepTheFrame));
}

SlowPathReturnType JIT_OPERATION operationLinkPolymorphicCall(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    ASSERT(callLinkInfo->specializationKind() == CodeForCall);
    JSCell* calleeAsFunctionCell;
    SlowPathReturnType result = virtualForWithFunction(execCallee, callLinkInfo, calleeAsFunctionCell);

    linkPolymorphicCall(execCallee, *callLinkInfo, CallVariant(calleeAsFunctionCell));
    
    return result;
}

SlowPathReturnType JIT_OPERATION operationVirtualCall(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    JSCell* calleeAsFunctionCellIgnored;
    return virtualForWithFunction(execCallee, callLinkInfo, calleeAsFunctionCellIgnored);
}

size_t JIT_OPERATION operationCompareLess(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return jsLess<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

size_t JIT_OPERATION operationCompareLessEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return jsLessEq<true>(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

size_t JIT_OPERATION operationCompareGreater(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return jsLess<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

size_t JIT_OPERATION operationCompareGreaterEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return jsLessEq<false>(exec, JSValue::decode(encodedOp2), JSValue::decode(encodedOp1));
}

size_t JIT_OPERATION operationConvertJSValueToBoolean(ExecState* exec, EncodedJSValue encodedOp)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return JSValue::decode(encodedOp).toBoolean(exec);
}

size_t JIT_OPERATION operationCompareEq(ExecState* exec, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::equalSlowCaseInline(exec, JSValue::decode(encodedOp1), JSValue::decode(encodedOp2));
}

#if USE(JSVALUE64)
EncodedJSValue JIT_OPERATION operationCompareStringEq(ExecState* exec, JSCell* left, JSCell* right)
#else
size_t JIT_OPERATION operationCompareStringEq(ExecState* exec, JSCell* left, JSCell* right)
#endif
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    bool result = WTF::equal(*asString(left)->value(exec).impl(), *asString(right)->value(exec).impl());
#if USE(JSVALUE64)
    return JSValue::encode(jsBoolean(result));
#else
    return result;
#endif
}

EncodedJSValue JIT_OPERATION operationNewArrayWithProfile(ExecState* exec, ArrayAllocationProfile* profile, const JSValue* values, int size)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    return JSValue::encode(constructArrayNegativeIndexed(exec, profile, values, size));
}

EncodedJSValue JIT_OPERATION operationNewArrayBufferWithProfile(ExecState* exec, ArrayAllocationProfile* profile, const JSValue* values, int size)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    return JSValue::encode(constructArray(exec, profile, values, size));
}

EncodedJSValue JIT_OPERATION operationNewArrayWithSizeAndProfile(ExecState* exec, ArrayAllocationProfile* profile, EncodedJSValue size)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    JSValue sizeValue = JSValue::decode(size);
    return JSValue::encode(constructArrayWithSizeQuirk(exec, profile, exec->lexicalGlobalObject(), sizeValue));
}

}

template<typename FunctionType>
static EncodedJSValue operationNewFunctionCommon(ExecState* exec, JSScope* scope, JSCell* functionExecutable, bool isInvalidated)
{
    ASSERT(functionExecutable->inherits(FunctionExecutable::info()));
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    if (isInvalidated)
        return JSValue::encode(FunctionType::createWithInvalidatedReallocationWatchpoint(vm, static_cast<FunctionExecutable*>(functionExecutable), scope));
    return JSValue::encode(FunctionType::create(vm, static_cast<FunctionExecutable*>(functionExecutable), scope));
}

extern "C" {

EncodedJSValue JIT_OPERATION operationNewFunction(ExecState* exec, JSScope* scope, JSCell* functionExecutable)
{
    return operationNewFunctionCommon<JSFunction>(exec, scope, functionExecutable, false);
}

EncodedJSValue JIT_OPERATION operationNewFunctionWithInvalidatedReallocationWatchpoint(ExecState* exec, JSScope* scope, JSCell* functionExecutable)
{
    return operationNewFunctionCommon<JSFunction>(exec, scope, functionExecutable, true);
}

EncodedJSValue JIT_OPERATION operationNewGeneratorFunction(ExecState* exec, JSScope* scope, JSCell* functionExecutable)
{
    return operationNewFunctionCommon<JSGeneratorFunction>(exec, scope, functionExecutable, false);
}

EncodedJSValue JIT_OPERATION operationNewGeneratorFunctionWithInvalidatedReallocationWatchpoint(ExecState* exec, JSScope* scope, JSCell* functionExecutable)
{
    return operationNewFunctionCommon<JSGeneratorFunction>(exec, scope, functionExecutable, true);
}

void JIT_OPERATION operationSetFunctionName(ExecState* exec, JSCell* funcCell, EncodedJSValue encodedName)
{
    JSFunction* func = jsCast<JSFunction*>(funcCell);
    JSValue name = JSValue::decode(encodedName);
    func->setFunctionName(exec, name);
}

JSCell* JIT_OPERATION operationNewObject(ExecState* exec, Structure* structure)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return constructEmptyObject(exec, structure);
}

EncodedJSValue JIT_OPERATION operationNewRegexp(ExecState* exec, void* regexpPtr)
{
    SuperSamplerScope superSamplerScope(false);
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    if (!regexp->isValid()) {
        vm.throwException(exec, createSyntaxError(exec, ASCIILiteral("Invalid flags supplied to RegExp constructor.")));
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(RegExpObject::create(vm, exec->lexicalGlobalObject()->regExpStructure(), regexp));
}

// The only reason for returning an UnusedPtr (instead of void) is so that we can reuse the
// existing DFG slow path generator machinery when creating the slow path for CheckWatchdogTimer
// in the DFG. If a DFG slow path generator that supports a void return type is added in the
// future, we can switch to using that then.
UnusedPtr JIT_OPERATION operationHandleWatchdogTimer(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (UNLIKELY(vm.shouldTriggerTermination(exec)))
        vm.throwException(exec, createTerminatedExecutionException(&vm));

    return nullptr;
}

void JIT_OPERATION operationThrowStaticError(ExecState* exec, EncodedJSValue encodedValue, int32_t referenceErrorFlag)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue errorMessageValue = JSValue::decode(encodedValue);
    RELEASE_ASSERT(errorMessageValue.isString());
    String errorMessage = asString(errorMessageValue)->value(exec);
    if (referenceErrorFlag)
        vm.throwException(exec, createReferenceError(exec, errorMessage));
    else
        vm.throwException(exec, createTypeError(exec, errorMessage));
}

void JIT_OPERATION operationDebug(ExecState* exec, int32_t debugHookID)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    vm.interpreter->debug(exec, static_cast<DebugHookID>(debugHookID));
}

#if ENABLE(DFG_JIT)
static void updateAllPredictionsAndOptimizeAfterWarmUp(CodeBlock* codeBlock)
{
    codeBlock->updateAllPredictions();
    codeBlock->optimizeAfterWarmUp();
}

SlowPathReturnType JIT_OPERATION operationOptimize(ExecState* exec, int32_t bytecodeIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    // Defer GC for a while so that it doesn't run between when we enter into this
    // slow path and when we figure out the state of our code block. This prevents
    // a number of awkward reentrancy scenarios, including:
    //
    // - The optimized version of our code block being jettisoned by GC right after
    //   we concluded that we wanted to use it, but have not planted it into the JS
    //   stack yet.
    //
    // - An optimized version of our code block being installed just as we decided
    //   that it wasn't ready yet.
    //
    // Note that jettisoning won't happen if we already initiated OSR, because in
    // that case we would have already planted the optimized code block into the JS
    // stack.
    DeferGCForAWhile deferGC(vm.heap);
    
    CodeBlock* codeBlock = exec->codeBlock();
    if (codeBlock->jitType() != JITCode::BaselineJIT) {
        dataLog("Unexpected code block in Baseline->DFG tier-up: ", *codeBlock, "\n");
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    if (bytecodeIndex) {
        // If we're attempting to OSR from a loop, assume that this should be
        // separately optimized.
        codeBlock->m_shouldAlwaysBeInlined = false;
    }

    if (Options::verboseOSR()) {
        dataLog(
            *codeBlock, ": Entered optimize with bytecodeIndex = ", bytecodeIndex,
            ", executeCounter = ", codeBlock->jitExecuteCounter(),
            ", optimizationDelayCounter = ", codeBlock->reoptimizationRetryCounter(),
            ", exitCounter = ");
        if (codeBlock->hasOptimizedReplacement())
            dataLog(codeBlock->replacement()->osrExitCounter());
        else
            dataLog("N/A");
        dataLog("\n");
    }

    if (!codeBlock->checkIfOptimizationThresholdReached()) {
        codeBlock->updateAllPredictions();
        if (Options::verboseOSR())
            dataLog("Choosing not to optimize ", *codeBlock, " yet, because the threshold hasn't been reached.\n");
        return encodeResult(0, 0);
    }
    
    if (vm.enabledProfiler()) {
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        return encodeResult(0, 0);
    }

    Debugger* debugger = codeBlock->globalObject()->debugger();
    if (debugger && (debugger->isStepping() || codeBlock->baselineAlternative()->hasDebuggerRequests())) {
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        return encodeResult(0, 0);
    }

    if (codeBlock->m_shouldAlwaysBeInlined) {
        updateAllPredictionsAndOptimizeAfterWarmUp(codeBlock);
        if (Options::verboseOSR())
            dataLog("Choosing not to optimize ", *codeBlock, " yet, because m_shouldAlwaysBeInlined == true.\n");
        return encodeResult(0, 0);
    }

    // We cannot be in the process of asynchronous compilation and also have an optimized
    // replacement.
    DFG::Worklist* worklist = DFG::existingGlobalDFGWorklistOrNull();
    ASSERT(
        !worklist
        || !(worklist->compilationState(DFG::CompilationKey(codeBlock, DFG::DFGMode)) != DFG::Worklist::NotKnown
        && codeBlock->hasOptimizedReplacement()));

    DFG::Worklist::State worklistState;
    if (worklist) {
        // The call to DFG::Worklist::completeAllReadyPlansForVM() will complete all ready
        // (i.e. compiled) code blocks. But if it completes ours, we also need to know
        // what the result was so that we don't plow ahead and attempt OSR or immediate
        // reoptimization. This will have already also set the appropriate JIT execution
        // count threshold depending on what happened, so if the compilation was anything
        // but successful we just want to return early. See the case for worklistState ==
        // DFG::Worklist::Compiled, below.
        
        // Note that we could have alternatively just called Worklist::compilationState()
        // here, and if it returned Compiled, we could have then called
        // completeAndScheduleOSR() below. But that would have meant that it could take
        // longer for code blocks to be completed: they would only complete when *their*
        // execution count trigger fired; but that could take a while since the firing is
        // racy. It could also mean that code blocks that never run again after being
        // compiled would sit on the worklist until next GC. That's fine, but it's
        // probably a waste of memory. Our goal here is to complete code blocks as soon as
        // possible in order to minimize the chances of us executing baseline code after
        // optimized code is already available.
        worklistState = worklist->completeAllReadyPlansForVM(
            vm, DFG::CompilationKey(codeBlock, DFG::DFGMode));
    } else
        worklistState = DFG::Worklist::NotKnown;

    if (worklistState == DFG::Worklist::Compiling) {
        // We cannot be in the process of asynchronous compilation and also have an optimized
        // replacement.
        RELEASE_ASSERT(!codeBlock->hasOptimizedReplacement());
        codeBlock->setOptimizationThresholdBasedOnCompilationResult(CompilationDeferred);
        return encodeResult(0, 0);
    }

    if (worklistState == DFG::Worklist::Compiled) {
        // If we don't have an optimized replacement but we did just get compiled, then
        // the compilation failed or was invalidated, in which case the execution count
        // thresholds have already been set appropriately by
        // CodeBlock::setOptimizationThresholdBasedOnCompilationResult() and we have
        // nothing left to do.
        if (!codeBlock->hasOptimizedReplacement()) {
            codeBlock->updateAllPredictions();
            if (Options::verboseOSR())
                dataLog("Code block ", *codeBlock, " was compiled but it doesn't have an optimized replacement.\n");
            return encodeResult(0, 0);
        }
    } else if (codeBlock->hasOptimizedReplacement()) {
        if (Options::verboseOSR())
            dataLog("Considering OSR ", *codeBlock, " -> ", *codeBlock->replacement(), ".\n");
        // If we have an optimized replacement, then it must be the case that we entered
        // cti_optimize from a loop. That's because if there's an optimized replacement,
        // then all calls to this function will be relinked to the replacement and so
        // the prologue OSR will never fire.
        
        // This is an interesting threshold check. Consider that a function OSR exits
        // in the middle of a loop, while having a relatively low exit count. The exit
        // will reset the execution counter to some target threshold, meaning that this
        // code won't be reached until that loop heats up for >=1000 executions. But then
        // we do a second check here, to see if we should either reoptimize, or just
        // attempt OSR entry. Hence it might even be correct for
        // shouldReoptimizeFromLoopNow() to always return true. But we make it do some
        // additional checking anyway, to reduce the amount of recompilation thrashing.
        if (codeBlock->replacement()->shouldReoptimizeFromLoopNow()) {
            if (Options::verboseOSR()) {
                dataLog(
                    "Triggering reoptimization of ", *codeBlock,
                    "(", *codeBlock->replacement(), ") (in loop).\n");
            }
            codeBlock->replacement()->jettison(Profiler::JettisonDueToBaselineLoopReoptimizationTrigger, CountReoptimization);
            return encodeResult(0, 0);
        }
    } else {
        if (!codeBlock->shouldOptimizeNow()) {
            if (Options::verboseOSR()) {
                dataLog(
                    "Delaying optimization for ", *codeBlock,
                    " because of insufficient profiling.\n");
            }
            return encodeResult(0, 0);
        }

        if (Options::verboseOSR())
            dataLog("Triggering optimized compilation of ", *codeBlock, "\n");

        unsigned numVarsWithValues;
        if (bytecodeIndex)
            numVarsWithValues = codeBlock->m_numVars;
        else
            numVarsWithValues = 0;
        Operands<JSValue> mustHandleValues(codeBlock->numParameters(), numVarsWithValues);
        int localsUsedForCalleeSaves = static_cast<int>(CodeBlock::llintBaselineCalleeSaveSpaceAsVirtualRegisters());
        for (size_t i = 0; i < mustHandleValues.size(); ++i) {
            int operand = mustHandleValues.operandForIndex(i);
            if (operandIsLocal(operand) && VirtualRegister(operand).toLocal() < localsUsedForCalleeSaves)
                continue;
            mustHandleValues[i] = exec->uncheckedR(operand).jsValue();
        }

        CodeBlock* replacementCodeBlock = codeBlock->newReplacement();
        CompilationResult result = DFG::compile(
            vm, replacementCodeBlock, nullptr, DFG::DFGMode, bytecodeIndex,
            mustHandleValues, JITToDFGDeferredCompilationCallback::create());
        
        if (result != CompilationSuccessful)
            return encodeResult(0, 0);
    }
    
    CodeBlock* optimizedCodeBlock = codeBlock->replacement();
    ASSERT(JITCode::isOptimizingJIT(optimizedCodeBlock->jitType()));
    
    if (void* dataBuffer = DFG::prepareOSREntry(exec, optimizedCodeBlock, bytecodeIndex)) {
        if (Options::verboseOSR()) {
            dataLog(
                "Performing OSR ", *codeBlock, " -> ", *optimizedCodeBlock, ".\n");
        }

        codeBlock->optimizeSoon();
        return encodeResult(vm.getCTIStub(DFG::osrEntryThunkGenerator).code().executableAddress(), dataBuffer);
    }

    if (Options::verboseOSR()) {
        dataLog(
            "Optimizing ", *codeBlock, " -> ", *codeBlock->replacement(),
            " succeeded, OSR failed, after a delay of ",
            codeBlock->optimizationDelayCounter(), ".\n");
    }

    // Count the OSR failure as a speculation failure. If this happens a lot, then
    // reoptimize.
    optimizedCodeBlock->countOSRExit();

    // We are a lot more conservative about triggering reoptimization after OSR failure than
    // before it. If we enter the optimize_from_loop trigger with a bucket full of fail
    // already, then we really would like to reoptimize immediately. But this case covers
    // something else: there weren't many (or any) speculation failures before, but we just
    // failed to enter the speculative code because some variable had the wrong value or
    // because the OSR code decided for any spurious reason that it did not want to OSR
    // right now. So, we only trigger reoptimization only upon the more conservative (non-loop)
    // reoptimization trigger.
    if (optimizedCodeBlock->shouldReoptimizeNow()) {
        if (Options::verboseOSR()) {
            dataLog(
                "Triggering reoptimization of ", *codeBlock, " -> ",
                *codeBlock->replacement(), " (after OSR fail).\n");
        }
        optimizedCodeBlock->jettison(Profiler::JettisonDueToBaselineLoopReoptimizationTriggerOnOSREntryFail, CountReoptimization);
        return encodeResult(0, 0);
    }

    // OSR failed this time, but it might succeed next time! Let the code run a bit
    // longer and then try again.
    codeBlock->optimizeAfterWarmUp();
    
    return encodeResult(0, 0);
}
#endif

void JIT_OPERATION operationPutByIndex(ExecState* exec, EncodedJSValue encodedArrayValue, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue arrayValue = JSValue::decode(encodedArrayValue);
    ASSERT(isJSArray(arrayValue));
    asArray(arrayValue)->putDirectIndex(exec, index, JSValue::decode(encodedValue));
}

enum class AccessorType {
    Getter,
    Setter
};

static void putAccessorByVal(ExecState* exec, JSObject* base, JSValue subscript, int32_t attribute, JSObject* accessor, AccessorType accessorType)
{
    auto propertyKey = subscript.toPropertyKey(exec);
    if (exec->hadException())
        return;

    if (accessorType == AccessorType::Getter)
        base->putGetter(exec, propertyKey, accessor, attribute);
    else
        base->putSetter(exec, propertyKey, accessor, attribute);
}

void JIT_OPERATION operationPutGetterById(ExecState* exec, JSCell* object, UniquedStringImpl* uid, int32_t options, JSCell* getter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(getter->isObject());
    baseObj->putGetter(exec, uid, getter, options);
}

void JIT_OPERATION operationPutSetterById(ExecState* exec, JSCell* object, UniquedStringImpl* uid, int32_t options, JSCell* setter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    ASSERT(setter->isObject());
    baseObj->putSetter(exec, uid, setter, options);
}

void JIT_OPERATION operationPutGetterByVal(ExecState* exec, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* getter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putAccessorByVal(exec, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(getter), AccessorType::Getter);
}

void JIT_OPERATION operationPutSetterByVal(ExecState* exec, JSCell* base, EncodedJSValue encodedSubscript, int32_t attribute, JSCell* setter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    putAccessorByVal(exec, asObject(base), JSValue::decode(encodedSubscript), attribute, asObject(setter), AccessorType::Setter);
}

#if USE(JSVALUE64)
void JIT_OPERATION operationPutGetterSetter(ExecState* exec, JSCell* object, UniquedStringImpl* uid, int32_t attribute, EncodedJSValue encodedGetterValue, EncodedJSValue encodedSetterValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(object && object->isObject());
    JSObject* baseObj = asObject(object);

    GetterSetter* accessor = GetterSetter::create(vm, exec->lexicalGlobalObject());

    JSValue getter = JSValue::decode(encodedGetterValue);
    JSValue setter = JSValue::decode(encodedSetterValue);
    ASSERT(getter.isObject() || getter.isUndefined());
    ASSERT(setter.isObject() || setter.isUndefined());
    ASSERT(getter.isObject() || setter.isObject());

    if (!getter.isUndefined())
        accessor->setGetter(vm, exec->lexicalGlobalObject(), asObject(getter));
    if (!setter.isUndefined())
        accessor->setSetter(vm, exec->lexicalGlobalObject(), asObject(setter));
    baseObj->putDirectAccessor(exec, uid, accessor, attribute);
}

#else
void JIT_OPERATION operationPutGetterSetter(ExecState* exec, JSCell* object, UniquedStringImpl* uid, int32_t attribute, JSCell* getter, JSCell* setter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(object && object->isObject());
    JSObject* baseObj = asObject(object);

    GetterSetter* accessor = GetterSetter::create(vm, exec->lexicalGlobalObject());

    ASSERT(!getter || getter->isObject());
    ASSERT(!setter || setter->isObject());
    ASSERT(getter || setter);

    if (getter)
        accessor->setGetter(vm, exec->lexicalGlobalObject(), getter->getObject());
    if (setter)
        accessor->setSetter(vm, exec->lexicalGlobalObject(), setter->getObject());
    baseObj->putDirectAccessor(exec, uid, accessor, attribute);
}
#endif

void JIT_OPERATION operationPopScope(ExecState* exec, int32_t scopeReg)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSScope* scope = exec->uncheckedR(scopeReg).Register::scope();
    exec->uncheckedR(scopeReg) = scope->next();
}

void JIT_OPERATION operationProfileDidCall(ExecState* exec, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (LegacyProfiler* profiler = vm.enabledProfiler())
        profiler->didExecute(exec, JSValue::decode(encodedValue));
}

void JIT_OPERATION operationProfileWillCall(ExecState* exec, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (LegacyProfiler* profiler = vm.enabledProfiler())
        profiler->willExecute(exec, JSValue::decode(encodedValue));
}

int32_t JIT_OPERATION operationInstanceOfCustom(ExecState* exec, EncodedJSValue encodedValue, JSObject* constructor, EncodedJSValue encodedHasInstance)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue value = JSValue::decode(encodedValue);
    JSValue hasInstanceValue = JSValue::decode(encodedHasInstance);

    ASSERT(hasInstanceValue != exec->lexicalGlobalObject()->functionProtoHasInstanceSymbolFunction() || !constructor->structure()->typeInfo().implementsDefaultHasInstance());

    if (constructor->hasInstance(exec, value, hasInstanceValue))
        return 1;
    return 0;
}

}

static bool canAccessArgumentIndexQuickly(JSObject& object, uint32_t index)
{
    switch (object.structure()->typeInfo().type()) {
    case DirectArgumentsType: {
        DirectArguments* directArguments = jsCast<DirectArguments*>(&object);
        if (directArguments->canAccessArgumentIndexQuicklyInDFG(index))
            return true;
        break;
    }
    case ScopedArgumentsType: {
        ScopedArguments* scopedArguments = jsCast<ScopedArguments*>(&object);
        if (scopedArguments->canAccessArgumentIndexQuicklyInDFG(index))
            return true;
        break;
    }
    default:
        break;
    }
    return false;
}

static JSValue getByVal(ExecState* exec, JSValue baseValue, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        VM& vm = exec->vm();
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            if (RefPtr<AtomicStringImpl> existingAtomicString = asString(subscript)->toExistingAtomicString(exec)) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomicString.get())) {
                    ASSERT(exec->bytecodeOffset());
                    if (byValInfo->stubInfo && byValInfo->cachedId.impl() != existingAtomicString)
                        byValInfo->tookSlowPath = true;
                    return result;
                }
            }
        }
    }

    if (subscript.isUInt32()) {
        ASSERT(exec->bytecodeOffset());
        byValInfo->tookSlowPath = true;

        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue)) {
            if (asString(baseValue)->canGetIndex(i)) {
                ctiPatchCallByReturnAddress(returnAddress, FunctionPtr(operationGetByValString));
                return asString(baseValue)->getIndex(exec, i);
            }
            byValInfo->arrayProfile->setOutOfBounds();
        } else if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canGetIndexQuickly(i))
                return object->getIndexQuickly(i);

            if (!canAccessArgumentIndexQuickly(*object, i)) {
                // FIXME: This will make us think that in-bounds typed array accesses are actually
                // out-of-bounds.
                // https://bugs.webkit.org/show_bug.cgi?id=149886
                byValInfo->arrayProfile->setOutOfBounds();
            }
        }

        return baseValue.get(exec, i);
    }

    baseValue.requireObjectCoercible(exec);
    if (exec->hadException())
        return jsUndefined();
    auto property = subscript.toPropertyKey(exec);
    if (exec->hadException())
        return jsUndefined();

    ASSERT(exec->bytecodeOffset());
    if (byValInfo->stubInfo && (!isStringOrSymbol(subscript) || byValInfo->cachedId != property))
        byValInfo->tookSlowPath = true;

    return baseValue.get(exec, property);
}

static OptimizationResult tryGetByValOptimize(ExecState* exec, JSValue baseValue, JSValue subscript, ByValInfo* byValInfo, ReturnAddressPtr returnAddress)
{
    // See if it's worth optimizing this at all.
    OptimizationResult optimizationResult = OptimizationResult::NotOptimized;

    VM& vm = exec->vm();

    if (baseValue.isObject() && subscript.isInt32()) {
        JSObject* object = asObject(baseValue);

        ASSERT(exec->bytecodeOffset());
        ASSERT(!byValInfo->stubRoutine);

        if (hasOptimizableIndexing(object->structure(vm))) {
            // Attempt to optimize.
            Structure* structure = object->structure(vm);
            JITArrayMode arrayMode = jitArrayModeForStructure(structure);
            if (arrayMode != byValInfo->arrayMode) {
                // If we reached this case, we got an interesting array mode we did not expect when we compiled.
                // Let's update the profile to do better next time.
                CodeBlock* codeBlock = exec->codeBlock();
                ConcurrentJITLocker locker(codeBlock->m_lock);
                byValInfo->arrayProfile->computeUpdatedPrediction(locker, codeBlock, structure);

                JIT::compileGetByVal(&vm, exec->codeBlock(), byValInfo, returnAddress, arrayMode);
                optimizationResult = OptimizationResult::Optimized;
            }
        }

        // If we failed to patch and we have some object that intercepts indexed get, then don't even wait until 10 times.
        if (optimizationResult != OptimizationResult::Optimized && object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero())
            optimizationResult = OptimizationResult::GiveUp;
    }

    if (baseValue.isObject() && isStringOrSymbol(subscript)) {
        const Identifier propertyName = subscript.toPropertyKey(exec);
        if (!subscript.isString() || !parseIndex(propertyName)) {
            ASSERT(exec->bytecodeOffset());
            ASSERT(!byValInfo->stubRoutine);
            if (byValInfo->seen) {
                if (byValInfo->cachedId == propertyName) {
                    JIT::compileGetByValWithCachedId(&vm, exec->codeBlock(), byValInfo, returnAddress, propertyName);
                    optimizationResult = OptimizationResult::Optimized;
                } else {
                    // Seem like a generic property access site.
                    optimizationResult = OptimizationResult::GiveUp;
                }
            } else {
                byValInfo->seen = true;
                byValInfo->cachedId = propertyName;
                optimizationResult = OptimizationResult::SeenOnce;
            }

        }
    }

    if (optimizationResult != OptimizationResult::Optimized && optimizationResult != OptimizationResult::SeenOnce) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. For cases where we see non-index-intercepting
        // objects, this gives 10 iterations worth of opportunity for us to observe
        // that the get_by_val may be polymorphic. We count up slowPathCount even if
        // the result is GiveUp.
        if (++byValInfo->slowPathCount >= 10)
            optimizationResult = OptimizationResult::GiveUp;
    }

    return optimizationResult;
}

extern "C" {

EncodedJSValue JIT_OPERATION operationGetByValGeneric(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    JSValue result = getByVal(exec, baseValue, subscript, byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS));
    return JSValue::encode(result);
}

EncodedJSValue JIT_OPERATION operationGetByValOptimize(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    ReturnAddressPtr returnAddress = ReturnAddressPtr(OUR_RETURN_ADDRESS);
    if (tryGetByValOptimize(exec, baseValue, subscript, byValInfo, returnAddress) == OptimizationResult::GiveUp) {
        // Don't ever try to optimize.
        byValInfo->tookSlowPath = true;
        ctiPatchCallByReturnAddress(returnAddress, FunctionPtr(operationGetByValGeneric));
    }

    return JSValue::encode(getByVal(exec, baseValue, subscript, byValInfo, returnAddress));
}

EncodedJSValue JIT_OPERATION operationHasIndexedPropertyDefault(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32());

    JSObject* object = asObject(baseValue);
    bool didOptimize = false;

    ASSERT(exec->bytecodeOffset());
    ASSERT(!byValInfo->stubRoutine);
    
    if (hasOptimizableIndexing(object->structure(vm))) {
        // Attempt to optimize.
        JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
        if (arrayMode != byValInfo->arrayMode) {
            JIT::compileHasIndexedProperty(&vm, exec->codeBlock(), byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
            didOptimize = true;
        }
    }
    
    if (!didOptimize) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. Or, if we failed to patch and we have some object
        // that intercepts indexed get, then don't even wait until 10 times. For cases
        // where we see non-index-intercepting objects, this gives 10 iterations worth of
        // opportunity for us to observe that the get_by_val may be polymorphic.
        if (++byValInfo->slowPathCount >= 10
            || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
            // Don't ever try to optimize.
            ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationHasIndexedPropertyGeneric));
        }
    }

    uint32_t index = subscript.asUInt32();
    if (object->canGetIndexQuickly(index))
        return JSValue::encode(JSValue(JSValue::JSTrue));

    if (!canAccessArgumentIndexQuickly(*object, index)) {
        // FIXME: This will make us think that in-bounds typed array accesses are actually
        // out-of-bounds.
        // https://bugs.webkit.org/show_bug.cgi?id=149886
        byValInfo->arrayProfile->setOutOfBounds();
    }
    return JSValue::encode(jsBoolean(object->hasPropertyGeneric(exec, index, PropertySlot::InternalMethodType::GetOwnProperty)));
}
    
EncodedJSValue JIT_OPERATION operationHasIndexedPropertyGeneric(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32());

    JSObject* object = asObject(baseValue);
    uint32_t index = subscript.asUInt32();
    if (object->canGetIndexQuickly(index))
        return JSValue::encode(JSValue(JSValue::JSTrue));

    if (!canAccessArgumentIndexQuickly(*object, index)) {
        // FIXME: This will make us think that in-bounds typed array accesses are actually
        // out-of-bounds.
        // https://bugs.webkit.org/show_bug.cgi?id=149886
        byValInfo->arrayProfile->setOutOfBounds();
    }
    return JSValue::encode(jsBoolean(object->hasPropertyGeneric(exec, subscript.asUInt32(), PropertySlot::InternalMethodType::GetOwnProperty)));
}
    
EncodedJSValue JIT_OPERATION operationGetByValString(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript, ByValInfo* byValInfo)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    JSValue result;
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            result = asString(baseValue)->getIndex(exec, i);
        else {
            result = baseValue.get(exec, i);
            if (!isJSString(baseValue)) {
                ASSERT(exec->bytecodeOffset());
                ctiPatchCallByReturnAddress(ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(byValInfo->stubRoutine ? operationGetByValGeneric : operationGetByValOptimize));
            }
        }
    } else {
        baseValue.requireObjectCoercible(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        auto property = subscript.toPropertyKey(exec);
        if (exec->hadException())
            return JSValue::encode(jsUndefined());
        result = baseValue.get(exec, property);
    }

    return JSValue::encode(result);
}

EncodedJSValue JIT_OPERATION operationDeleteById(ExecState* exec, EncodedJSValue encodedBase, const Identifier* identifier)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* baseObj = JSValue::decode(encodedBase).toObject(exec);
    if (!baseObj)
        JSValue::encode(JSValue());
    bool couldDelete = baseObj->methodTable(vm)->deleteProperty(baseObj, exec, *identifier);
    JSValue result = jsBoolean(couldDelete);
    if (!couldDelete && exec->codeBlock()->isStrictMode())
        vm.throwException(exec, createTypeError(exec, ASCIILiteral("Unable to delete property.")));
    return JSValue::encode(result);
}

EncodedJSValue JIT_OPERATION operationInstanceOf(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedProto)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue value = JSValue::decode(encodedValue);
    JSValue proto = JSValue::decode(encodedProto);
    
    bool result = JSObject::defaultHasInstance(exec, value, proto);
    return JSValue::encode(jsBoolean(result));
}

int32_t JIT_OPERATION operationSizeFrameForVarargs(ExecState* exec, EncodedJSValue encodedArguments, int32_t numUsedStackSlots, int32_t firstVarArgOffset)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSStack* stack = &exec->interpreter()->stack();
    JSValue arguments = JSValue::decode(encodedArguments);
    return sizeFrameForVarargs(exec, stack, arguments, numUsedStackSlots, firstVarArgOffset);
}

CallFrame* JIT_OPERATION operationSetupVarargsFrame(ExecState* exec, CallFrame* newCallFrame, EncodedJSValue encodedArguments, int32_t firstVarArgOffset, int32_t length)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue arguments = JSValue::decode(encodedArguments);
    setupVarargsFrame(exec, newCallFrame, arguments, firstVarArgOffset, length);
    return newCallFrame;
}

EncodedJSValue JIT_OPERATION operationToObject(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSObject* obj = JSValue::decode(value).toObject(exec);
    if (!obj)
        return JSValue::encode(JSValue());
    return JSValue::encode(obj);
}

char* JIT_OPERATION operationSwitchCharWithUnknownKeyType(ExecState* exec, EncodedJSValue encodedKey, size_t tableIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = exec->codeBlock();

    SimpleJumpTable& jumpTable = codeBlock->switchJumpTable(tableIndex);
    void* result = jumpTable.ctiDefault.executableAddress();

    if (key.isString()) {
        StringImpl* value = asString(key)->value(exec).impl();
        if (value->length() == 1)
            result = jumpTable.ctiForValue((*value)[0]).executableAddress();
    }

    return reinterpret_cast<char*>(result);
}

char* JIT_OPERATION operationSwitchImmWithUnknownKeyType(ExecState* exec, EncodedJSValue encodedKey, size_t tableIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = exec->codeBlock();

    SimpleJumpTable& jumpTable = codeBlock->switchJumpTable(tableIndex);
    void* result;
    if (key.isInt32())
        result = jumpTable.ctiForValue(key.asInt32()).executableAddress();
    else if (key.isDouble() && key.asDouble() == static_cast<int32_t>(key.asDouble()))
        result = jumpTable.ctiForValue(static_cast<int32_t>(key.asDouble())).executableAddress();
    else
        result = jumpTable.ctiDefault.executableAddress();
    return reinterpret_cast<char*>(result);
}

char* JIT_OPERATION operationSwitchStringWithUnknownKeyType(ExecState* exec, EncodedJSValue encodedKey, size_t tableIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue key = JSValue::decode(encodedKey);
    CodeBlock* codeBlock = exec->codeBlock();

    void* result;
    StringJumpTable& jumpTable = codeBlock->stringSwitchJumpTable(tableIndex);

    if (key.isString()) {
        StringImpl* value = asString(key)->value(exec).impl();
        result = jumpTable.ctiForValue(value).executableAddress();
    } else
        result = jumpTable.ctiDefault.executableAddress();

    return reinterpret_cast<char*>(result);
}

EncodedJSValue JIT_OPERATION operationGetFromScope(ExecState* exec, Instruction* bytecodePC)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    CodeBlock* codeBlock = exec->codeBlock();
    Instruction* pc = bytecodePC;

    const Identifier& ident = codeBlock->identifier(pc[3].u.operand);
    JSObject* scope = jsCast<JSObject*>(exec->uncheckedR(pc[2].u.operand).jsValue());
    GetPutInfo getPutInfo(pc[4].u.operand);

    // ModuleVar is always converted to ClosureVar for get_from_scope.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
    if (!scope->getPropertySlot(exec, ident, slot)) {
        if (getPutInfo.resolveMode() == ThrowIfNotFound)
            vm.throwException(exec, createUndefinedVariableError(exec, ident));
        return JSValue::encode(jsUndefined());
    }

    JSValue result = JSValue();
    if (scope->isGlobalLexicalEnvironment()) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        result = slot.getValue(exec, ident);
        if (result == jsTDZValue()) {
            exec->vm().throwException(exec, createTDZError(exec));
            return JSValue::encode(jsUndefined());
        }
    }

    CommonSlowPaths::tryCacheGetFromScopeGlobal(exec, vm, pc, scope, slot, ident);

    if (!result)
        result = slot.getValue(exec, ident);
    return JSValue::encode(result);
}

void JIT_OPERATION operationPutToScope(ExecState* exec, Instruction* bytecodePC)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    Instruction* pc = bytecodePC;

    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(pc[2].u.operand);
    JSObject* scope = jsCast<JSObject*>(exec->uncheckedR(pc[1].u.operand).jsValue());
    JSValue value = exec->r(pc[3].u.operand).jsValue();
    GetPutInfo getPutInfo = GetPutInfo(pc[4].u.operand);

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(getPutInfo.resolveType() != ModuleVar);

    if (getPutInfo.resolveType() == LocalClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(scope);
        environment->variableAt(ScopeOffset(pc[6].u.operand)).set(vm, environment, value);
        if (WatchpointSet* set = pc[5].u.watchpointSet)
            set->touch("Executed op_put_scope<LocalClosureVar>");
        return;
    }

    bool hasProperty = scope->hasProperty(exec, ident);
    if (hasProperty
        && scope->isGlobalLexicalEnvironment()
        && getPutInfo.initializationMode() != Initialization) {
        // When we can't statically prove we need a TDZ check, we must perform the check on the slow path.
        PropertySlot slot(scope, PropertySlot::InternalMethodType::Get);
        JSGlobalLexicalEnvironment::getOwnPropertySlot(scope, exec, ident, slot);
        if (slot.getValue(exec, ident) == jsTDZValue()) {
            exec->vm().throwException(exec, createTDZError(exec));
            return;
        }
    }

    if (getPutInfo.resolveMode() == ThrowIfNotFound && !hasProperty) {
        exec->vm().throwException(exec, createUndefinedVariableError(exec, ident));
        return;
    }

    PutPropertySlot slot(scope, codeBlock->isStrictMode(), PutPropertySlot::UnknownContext, getPutInfo.initializationMode() == Initialization);
    scope->methodTable()->put(scope, exec, ident, value, slot);
    
    if (exec->vm().exception())
        return;

    CommonSlowPaths::tryCachePutToScopeGlobal(exec, codeBlock, pc, scope, getPutInfo, slot, ident);
}

void JIT_OPERATION operationThrow(ExecState* exec, EncodedJSValue encodedExceptionValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue exceptionValue = JSValue::decode(encodedExceptionValue);
    vm->throwException(exec, exceptionValue);

    // Results stored out-of-band in vm.targetMachinePCForThrow & vm.callFrameForCatch
    genericUnwind(vm, exec);
}

void JIT_OPERATION operationFlushWriteBarrierBuffer(ExecState* exec, JSCell* cell)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    vm->heap.flushWriteBarrierBuffer(cell);
}

void JIT_OPERATION operationOSRWriteBarrier(ExecState* exec, JSCell* cell)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    vm->heap.writeBarrier(cell);
}

// NB: We don't include the value as part of the barrier because the write barrier elision
// phase in the DFG only tracks whether the object being stored to has been barriered. It 
// would be much more complicated to try to model the value being stored as well.
void JIT_OPERATION operationUnconditionalWriteBarrier(ExecState* exec, JSCell* cell)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    vm->heap.writeBarrier(cell);
}

void JIT_OPERATION operationInitGlobalConst(ExecState* exec, Instruction* pc)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue value = exec->r(pc[2].u.operand).jsValue();
    pc[1].u.variablePointer->set(*vm, exec->codeBlock()->globalObject(), value);
}

void JIT_OPERATION lookupExceptionHandler(VM* vm, ExecState* exec)
{
    NativeCallFrameTracer tracer(vm, exec);
    genericUnwind(vm, exec);
    ASSERT(vm->targetMachinePCForThrow);
}

void JIT_OPERATION lookupExceptionHandlerFromCallerFrame(VM* vm, ExecState* exec)
{
    NativeCallFrameTracer tracer(vm, exec);
    genericUnwind(vm, exec, UnwindFromCallerFrame);
    ASSERT(vm->targetMachinePCForThrow);
}

void JIT_OPERATION operationVMHandleException(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    genericUnwind(vm, exec);
}

// This function "should" just take the ExecState*, but doing so would make it more difficult
// to call from exception check sites. So, unlike all of our other functions, we allow
// ourselves to play some gnarly ABI tricks just to simplify the calling convention. This is
// particularly safe here since this is never called on the critical path - it's only for
// testing.
void JIT_OPERATION operationExceptionFuzz(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
#if COMPILER(GCC_OR_CLANG)
    void* returnPC = __builtin_return_address(0);
    doExceptionFuzzing(exec, "JITOperations", returnPC);
#endif // COMPILER(GCC_OR_CLANG)
}

EncodedJSValue JIT_OPERATION operationHasGenericProperty(ExecState* exec, EncodedJSValue encodedBaseValue, JSCell* propertyName)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    if (baseValue.isUndefinedOrNull())
        return JSValue::encode(jsBoolean(false));

    JSObject* base = baseValue.toObject(exec);
    if (!base)
        return JSValue::encode(JSValue());
    return JSValue::encode(jsBoolean(base->hasPropertyGeneric(exec, asString(propertyName)->toIdentifier(exec), PropertySlot::InternalMethodType::GetOwnProperty)));
}

EncodedJSValue JIT_OPERATION operationHasIndexedProperty(ExecState* exec, JSCell* baseCell, int32_t subscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSObject* object = baseCell->toObject(exec, exec->lexicalGlobalObject());
    return JSValue::encode(jsBoolean(object->hasPropertyGeneric(exec, subscript, PropertySlot::InternalMethodType::GetOwnProperty)));
}
    
JSCell* JIT_OPERATION operationGetPropertyEnumerator(ExecState* exec, JSCell* cell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* base = cell->toObject(exec, exec->lexicalGlobalObject());

    return propertyNameEnumerator(exec, base);
}

EncodedJSValue JIT_OPERATION operationNextEnumeratorPname(ExecState* exec, JSCell* enumeratorCell, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(enumeratorCell);
    JSString* propertyName = enumerator->propertyNameAtIndex(index);
    return JSValue::encode(propertyName ? propertyName : jsNull());
}

JSCell* JIT_OPERATION operationToIndexString(ExecState* exec, int32_t index)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return jsString(exec, Identifier::from(exec, index).string());
}

void JIT_OPERATION operationProcessTypeProfilerLog(ExecState* exec)
{
    exec->vm().typeProfilerLog()->processLogEntries(ASCIILiteral("Log Full, called from inside baseline JIT"));
}

int32_t JIT_OPERATION operationCheckIfExceptionIsUncatchableAndNotifyProfiler(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    RELEASE_ASSERT(!!vm.exception());

    if (LegacyProfiler* profiler = vm.enabledProfiler())
        profiler->exceptionUnwind(exec);

    if (isTerminatedExecutionException(vm.exception())) {
        genericUnwind(&vm, exec);
        return 1;
    } else
        return 0;
}

} // extern "C"

// Note: getHostCallReturnValueWithExecState() needs to be placed before the
// definition of getHostCallReturnValue() below because the Windows build
// requires it.
extern "C" EncodedJSValue HOST_CALL_RETURN_VALUE_OPTION getHostCallReturnValueWithExecState(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(JSValue());
    return JSValue::encode(exec->vm().hostCallReturnValue);
}

#if COMPILER(GCC_OR_CLANG) && CPU(X86_64)
asm (
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "lea -8(%rsp), %rdi\n"
    "jmp " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC_OR_CLANG) && CPU(X86)
asm (
".text" "\n" \
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "push %ebp\n"
    "mov %esp, %eax\n"
    "leal -4(%esp), %esp\n"
    "push %eax\n"
    "call " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
    "leal 8(%esp), %esp\n"
    "pop %ebp\n"
    "ret\n"
);

#elif COMPILER(GCC_OR_CLANG) && CPU(ARM_THUMB2)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "sub r0, sp, #8" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC_OR_CLANG) && CPU(ARM_TRADITIONAL)
asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
INLINE_ARM_FUNCTION(getHostCallReturnValue)
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "sub r0, sp, #8" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif CPU(ARM64)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
     "sub x0, sp, #16" "\n"
     "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC_OR_CLANG) && CPU(MIPS)

#if WTF_MIPS_PIC
#define LOAD_FUNCTION_TO_T9(function) \
        ".set noreorder" "\n" \
        ".cpload $25" "\n" \
        ".set reorder" "\n" \
        "la $t9, " LOCAL_REFERENCE(function) "\n"
#else
#define LOAD_FUNCTION_TO_T9(function) "" "\n"
#endif

asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    LOAD_FUNCTION_TO_T9(getHostCallReturnValueWithExecState)
    "addi $a0, $sp, -8" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC_OR_CLANG) && CPU(SH4)

#define SH4_SCRATCH_REGISTER "r11"

asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov r15, r4" "\n"
    "add -8, r4" "\n"
    "mov.l 2f, " SH4_SCRATCH_REGISTER "\n"
    "braf " SH4_SCRATCH_REGISTER "\n"
    "nop" "\n"
    "1: .balign 4" "\n"
    "2: .long " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "-1b\n"
);

#elif COMPILER(MSVC) && CPU(X86)
extern "C" {
    __declspec(naked) EncodedJSValue HOST_CALL_RETURN_VALUE_OPTION getHostCallReturnValue()
    {
        __asm lea eax, [esp - 4]
        __asm mov [esp + 4], eax;
        __asm jmp getHostCallReturnValueWithExecState
    }
}
#endif

} // namespace JSC

#endif // ENABLE(JIT)
