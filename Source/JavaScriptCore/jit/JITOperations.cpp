/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "DFGCompilationMode.h"
#include "DFGDriver.h"
#include "DFGOSREntry.h"
#include "DFGThunks.h"
#include "DFGWorklist.h"
#include "Debugger.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "ExceptionFuzz.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "JIT.h"
#include "JITToDFGDeferredCompilationCallback.h"
#include "JSCInlines.h"
#include "JSGlobalObjectFunctions.h"
#include "JSNameScope.h"
#include "JSPropertyNameEnumerator.h"
#include "JSStackInlines.h"
#include "JSWithScope.h"
#include "LegacyProfiler.h"
#include "ObjectConstructor.h"
#include "Repatch.h"
#include "RepatchBuffer.h"
#include "TestRunnerUtils.h"
#include "TypeProfilerLog.h"
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
    ErrorHandlingScope errorScope(*vm);
    vm->throwException(callerFrame, createStackOverflowError(callerFrame));
}

int32_t JIT_OPERATION operationCallArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);

    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForCall);
    if (missingArgCount < 0) {
        NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
        throwStackOverflowError(callerFrame);
    }

    return missingArgCount;
}

int32_t JIT_OPERATION operationConstructArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);

    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForConstruct);
    if (missingArgCount < 0) {
        NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);
        throwStackOverflowError(callerFrame);
    }

    return missingArgCount;
}

EncodedJSValue JIT_OPERATION operationGetById(ExecState* exec, StructureStubInfo*, EncodedJSValue base, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    Identifier ident(vm, uid);
    return JSValue::encode(baseValue.get(exec, ident, slot));
}

EncodedJSValue JIT_OPERATION operationGetByIdBuildList(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue base, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    bool hasResult = baseValue.getPropertySlot(exec, ident, slot);
    
    if (accessType == static_cast<AccessType>(stubInfo->accessType))
        buildGetByIDList(exec, baseValue, ident, slot, *stubInfo);

    return JSValue::encode(hasResult? slot.getValue(exec, ident) : jsUndefined());
}

EncodedJSValue JIT_OPERATION operationGetByIdOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue base, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    Identifier ident = uid->isUnique() ? Identifier::from(PrivateName(uid)) : Identifier(vm, uid);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    
    bool hasResult = baseValue.getPropertySlot(exec, ident, slot);
    if (stubInfo->seen)
        repatchGetByID(exec, baseValue, ident, slot, *stubInfo);
    else
        stubInfo->seen = true;
    
    return JSValue::encode(hasResult? slot.getValue(exec, ident) : jsUndefined());

}

EncodedJSValue JIT_OPERATION operationInOptimize(ExecState* exec, StructureStubInfo* stubInfo, JSCell* base, StringImpl* key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (!base->isObject()) {
        vm->throwException(exec, createInvalidParameterError(exec, "in", base));
        return JSValue::encode(jsUndefined());
    }
    
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    Identifier ident(vm, key);
    PropertySlot slot(base);
    bool result = asObject(base)->getPropertySlot(exec, ident, slot);
    
    RELEASE_ASSERT(accessType == stubInfo->accessType);
    
    if (stubInfo->seen)
        repatchIn(exec, base, ident, result, slot, *stubInfo);
    else
        stubInfo->seen = true;
    
    return JSValue::encode(jsBoolean(result));
}

EncodedJSValue JIT_OPERATION operationIn(ExecState* exec, StructureStubInfo*, JSCell* base, StringImpl* key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    if (!base->isObject()) {
        vm->throwException(exec, createInvalidParameterError(exec, "in", base));
        return JSValue::encode(jsUndefined());
    }

    Identifier ident(vm, key);
    return JSValue::encode(jsBoolean(asObject(base)->hasProperty(exec, ident)));
}

EncodedJSValue JIT_OPERATION operationGenericIn(ExecState* exec, JSCell* base, EncodedJSValue key)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(jsBoolean(CommonSlowPaths::opIn(exec, JSValue::decode(key), base)));
}

void JIT_OPERATION operationPutByIdStrict(ExecState* exec, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), true, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).put(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdNonStrict(ExecState* exec, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), false, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).put(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectStrict(ExecState* exec, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), true, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectNonStrict(ExecState* exec, StructureStubInfo*, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(JSValue::decode(encodedBase), false, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, true, exec->codeBlock()->putByIdContext());

    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr;
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->seen)
        repatchPutByID(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
    else
        stubInfo->seen = true;
}

void JIT_OPERATION operationPutByIdNonStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, false, exec->codeBlock()->putByIdContext());

    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr;    
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->seen)
        repatchPutByID(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
    else
        stubInfo->seen = true;
}

void JIT_OPERATION operationPutByIdDirectStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, true, exec->codeBlock()->putByIdContext());
    
    Structure* structure = baseObject->structure(*vm);
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->seen)
        repatchPutByID(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
    else
        stubInfo->seen = true;
}

void JIT_OPERATION operationPutByIdDirectNonStrictOptimize(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, false, exec->codeBlock()->putByIdContext());
    
    Structure* structure = baseObject->structure(*vm);
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    if (stubInfo->seen)
        repatchPutByID(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
    else
        stubInfo->seen = true;
}

void JIT_OPERATION operationPutByIdStrictBuildList(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, true, exec->codeBlock()->putByIdContext());
    
    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr; 
    baseValue.put(exec, ident, value, slot);

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;

    buildPutByIdList(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
}

void JIT_OPERATION operationPutByIdNonStrictBuildList(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(baseValue, false, exec->codeBlock()->putByIdContext());

    Structure* structure = baseValue.isCell() ? baseValue.asCell()->structure(*vm) : nullptr;
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    buildPutByIdList(exec, baseValue, structure, ident, slot, *stubInfo, NotDirect);
}

void JIT_OPERATION operationPutByIdDirectStrictBuildList(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);
    
    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, true, exec->codeBlock()->putByIdContext());

    Structure* structure = baseObject->structure(*vm);    
    baseObject->putDirect(*vm, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    buildPutByIdList(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
}

void JIT_OPERATION operationPutByIdDirectNonStrictBuildList(ExecState* exec, StructureStubInfo* stubInfo, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    AccessType accessType = static_cast<AccessType>(stubInfo->accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(baseObject, false, exec->codeBlock()->putByIdContext());

    Structure* structure = baseObject->structure(*vm);    
    baseObject->putDirect(*vm, ident, value, slot);

    if (accessType != static_cast<AccessType>(stubInfo->accessType))
        return;
    
    buildPutByIdList(exec, baseObject, structure, ident, slot, *stubInfo, Direct);
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

static void putByVal(CallFrame* callFrame, JSValue baseValue, JSValue subscript, JSValue value)
{
    VM& vm = callFrame->vm();
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        if (baseValue.isObject()) {
            JSObject* object = asObject(baseValue);
            if (object->canSetIndexQuickly(i))
                object->setIndexQuickly(callFrame->vm(), i, value);
            else
                object->methodTable(vm)->putByIndex(object, callFrame, i, value, callFrame->codeBlock()->isStrictMode());
        } else
            baseValue.putByIndex(callFrame, i, value, callFrame->codeBlock()->isStrictMode());
    } else {
        PropertyName property = subscript.toPropertyKey(callFrame);
        if (!callFrame->vm().exception()) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot(baseValue, callFrame->codeBlock()->isStrictMode());
            baseValue.put(callFrame, property, value, slot);
        }
    }
}

static void directPutByVal(CallFrame* callFrame, JSObject* baseObject, JSValue subscript, JSValue value)
{
    if (LIKELY(subscript.isUInt32())) {
        uint32_t i = subscript.asUInt32();
        baseObject->putDirectIndex(callFrame, i, value);
    } else {
        PropertyName property = subscript.toPropertyKey(callFrame);
        if (!callFrame->vm().exception()) { // Don't put to an object if toString threw an exception.
            PutPropertySlot slot(baseObject, callFrame->codeBlock()->isStrictMode());
            baseObject->putDirect(callFrame->vm(), property, value, slot);
        }
    }
}
void JIT_OPERATION operationPutByVal(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    if (baseValue.isObject() && subscript.isInt32()) {
        // See if it's worth optimizing at all.
        JSObject* object = asObject(baseValue);
        bool didOptimize = false;

        unsigned bytecodeOffset = exec->locationAsBytecodeOffset();
        ASSERT(bytecodeOffset);
        ByValInfo& byValInfo = exec->codeBlock()->getByValInfo(bytecodeOffset - 1);
        ASSERT(!byValInfo.stubRoutine);

        if (hasOptimizableIndexing(object->structure(vm))) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
            if (arrayMode != byValInfo.arrayMode) {
                JIT::compilePutByVal(&vm, exec->codeBlock(), &byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
                didOptimize = true;
            }
        }

        if (!didOptimize) {
            // If we take slow path more than 10 times without patching then make sure we
            // never make that mistake again. Or, if we failed to patch and we have some object
            // that intercepts indexed get, then don't even wait until 10 times. For cases
            // where we see non-index-intercepting objects, this gives 10 iterations worth of
            // opportunity for us to observe that the get_by_val may be polymorphic.
            if (++byValInfo.slowPathCount >= 10
                || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
                // Don't ever try to optimize.
                ctiPatchCallByReturnAddress(exec->codeBlock(), ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationPutByValGeneric));
            }
        }
    }

    putByVal(exec, baseValue, subscript, value);
}

void JIT_OPERATION operationDirectPutByVal(ExecState* callFrame, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = callFrame->vm();
    NativeCallFrameTracer tracer(&vm, callFrame);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    JSObject* object = asObject(baseValue);
    if (subscript.isInt32()) {
        // See if it's worth optimizing at all.
        bool didOptimize = false;
        
        unsigned bytecodeOffset = callFrame->locationAsBytecodeOffset();
        ASSERT(bytecodeOffset);
        ByValInfo& byValInfo = callFrame->codeBlock()->getByValInfo(bytecodeOffset - 1);
        ASSERT(!byValInfo.stubRoutine);
        
        if (hasOptimizableIndexing(object->structure(vm))) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
            if (arrayMode != byValInfo.arrayMode) {
                JIT::compileDirectPutByVal(&vm, callFrame->codeBlock(), &byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
                didOptimize = true;
            }
        }
        
        if (!didOptimize) {
            // If we take slow path more than 10 times without patching then make sure we
            // never make that mistake again. Or, if we failed to patch and we have some object
            // that intercepts indexed get, then don't even wait until 10 times. For cases
            // where we see non-index-intercepting objects, this gives 10 iterations worth of
            // opportunity for us to observe that the get_by_val may be polymorphic.
            if (++byValInfo.slowPathCount >= 10
                || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
                // Don't ever try to optimize.
                ctiPatchCallByReturnAddress(callFrame->codeBlock(), ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationDirectPutByValGeneric));
            }
        }
    }
    directPutByVal(callFrame, object, subscript, value);
}

void JIT_OPERATION operationPutByValGeneric(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);

    putByVal(exec, baseValue, subscript, value);
}


void JIT_OPERATION operationDirectPutByValGeneric(ExecState* exec, EncodedJSValue encodedBaseValue, EncodedJSValue encodedSubscript, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    JSValue subscript = JSValue::decode(encodedSubscript);
    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(baseValue.isObject());
    directPutByVal(exec, asObject(baseValue), subscript, value);
}

EncodedJSValue JIT_OPERATION operationCallEval(ExecState* exec, ExecState* execCallee)
{

    ASSERT_UNUSED(exec, exec->codeBlock()->codeType() != FunctionCode
        || !exec->codeBlock()->needsActivation()
        || exec->hasActivation());

    execCallee->setCodeBlock(0);

    if (!isHostFunction(execCallee->calleeAsValue(), globalFuncEval))
        return JSValue::encode(JSValue());

    VM* vm = &execCallee->vm();
    JSValue result = eval(execCallee);
    if (vm->exception())
        return EncodedJSValue();
    
    return JSValue::encode(result);
}

static void* handleHostCall(ExecState* execCallee, JSValue callee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();

    execCallee->setCodeBlock(0);

    if (kind == CodeForCall) {
        CallData callData;
        CallType callType = getCallData(callee, callData);
    
        ASSERT(callType != CallTypeJS);
    
        if (callType == CallTypeHost) {
            NativeCallFrameTracer tracer(vm, execCallee);
            execCallee->setCallee(asObject(callee));
            vm->hostCallReturnValue = JSValue::decode(callData.native.function(execCallee));
            if (vm->exception())
                return vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress();

            return reinterpret_cast<void*>(getHostCallReturnValue);
        }
    
        ASSERT(callType == CallTypeNone);
        exec->vm().throwException(exec, createNotAFunctionError(exec, callee));
        return vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress();
    }

    ASSERT(kind == CodeForConstruct);
    
    ConstructData constructData;
    ConstructType constructType = getConstructData(callee, constructData);
    
    ASSERT(constructType != ConstructTypeJS);
    
    if (constructType == ConstructTypeHost) {
        NativeCallFrameTracer tracer(vm, execCallee);
        execCallee->setCallee(asObject(callee));
        vm->hostCallReturnValue = JSValue::decode(constructData.native.function(execCallee));
        if (vm->exception())
            return vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress();

        return reinterpret_cast<void*>(getHostCallReturnValue);
    }
    
    ASSERT(constructType == ConstructTypeNone);
    exec->vm().throwException(exec, createNotAConstructorError(exec, callee));
    return vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress();
}

inline char* linkFor(
    ExecState* execCallee, CallLinkInfo* callLinkInfo, CodeSpecializationKind kind,
    RegisterPreservationMode registers)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell)
        return reinterpret_cast<char*>(handleHostCall(execCallee, calleeAsValue, kind));

    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = callee->scopeUnchecked();
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    if (executable->isHostFunction())
        codePtr = executable->entrypointFor(*vm, kind, MustCheckArity, registers);
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->prepareForExecution(execCallee, callee, &scope, kind);
        if (error) {
            throwStackOverflowError(exec);
            return reinterpret_cast<char*>(vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress());
        }
        codeBlock = functionExecutable->codeBlockFor(kind);
        ArityCheckMode arity;
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo->callType == CallLinkInfo::CallVarargs || callLinkInfo->callType == CallLinkInfo::ConstructVarargs)
            arity = MustCheckArity;
        else
            arity = ArityCheckNotRequired;
        codePtr = functionExecutable->entrypointFor(*vm, kind, arity, registers);
    }
    if (!callLinkInfo->seenOnce())
        callLinkInfo->setSeen();
    else
        linkFor(execCallee, *callLinkInfo, codeBlock, callee, codePtr, kind, registers);
    return reinterpret_cast<char*>(codePtr.executableAddress());
}

char* JIT_OPERATION operationLinkCall(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    return linkFor(execCallee, callLinkInfo, CodeForCall, RegisterPreservationNotRequired);
}

char* JIT_OPERATION operationLinkConstruct(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    return linkFor(execCallee, callLinkInfo, CodeForConstruct, RegisterPreservationNotRequired);
}

char* JIT_OPERATION operationLinkCallThatPreservesRegs(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    return linkFor(execCallee, callLinkInfo, CodeForCall, MustPreserveRegisters);
}

char* JIT_OPERATION operationLinkConstructThatPreservesRegs(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    return linkFor(execCallee, callLinkInfo, CodeForConstruct, MustPreserveRegisters);
}

inline char* virtualForWithFunction(
    ExecState* execCallee, CodeSpecializationKind kind, RegisterPreservationMode registers,
    JSCell*& calleeAsFunctionCell)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue calleeAsValue = execCallee->calleeAsValue();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell))
        return reinterpret_cast<char*>(handleHostCall(execCallee, calleeAsValue, kind));
    
    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSScope* scope = function->scopeUnchecked();
    ExecutableBase* executable = function->executable();
    if (UNLIKELY(!executable->hasJITCodeFor(kind))) {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->prepareForExecution(execCallee, function, &scope, kind);
        if (error) {
            exec->vm().throwException(exec, error);
            return reinterpret_cast<char*>(vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress());
        }
    }
    return reinterpret_cast<char*>(executable->entrypointFor(
        *vm, kind, MustCheckArity, registers).executableAddress());
}

inline char* virtualFor(
    ExecState* execCallee, CodeSpecializationKind kind, RegisterPreservationMode registers)
{
    JSCell* calleeAsFunctionCellIgnored;
    return virtualForWithFunction(execCallee, kind, registers, calleeAsFunctionCellIgnored);
}

char* JIT_OPERATION operationLinkPolymorphicCall(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    JSCell* calleeAsFunctionCell;
    char* result = virtualForWithFunction(execCallee, CodeForCall, RegisterPreservationNotRequired, calleeAsFunctionCell);

    linkPolymorphicCall(execCallee, *callLinkInfo, CallVariant(calleeAsFunctionCell), RegisterPreservationNotRequired);
    
    return result;
}

char* JIT_OPERATION operationVirtualCall(ExecState* execCallee, CallLinkInfo*)
{    
    return virtualFor(execCallee, CodeForCall, RegisterPreservationNotRequired);
}

char* JIT_OPERATION operationVirtualConstruct(ExecState* execCallee, CallLinkInfo*)
{
    return virtualFor(execCallee, CodeForConstruct, RegisterPreservationNotRequired);
}

char* JIT_OPERATION operationLinkPolymorphicCallThatPreservesRegs(ExecState* execCallee, CallLinkInfo* callLinkInfo)
{
    JSCell* calleeAsFunctionCell;
    char* result = virtualForWithFunction(execCallee, CodeForCall, MustPreserveRegisters, calleeAsFunctionCell);

    linkPolymorphicCall(execCallee, *callLinkInfo, CallVariant(calleeAsFunctionCell), MustPreserveRegisters);
    
    return result;
}

char* JIT_OPERATION operationVirtualCallThatPreservesRegs(ExecState* execCallee, CallLinkInfo*)
{    
    return virtualFor(execCallee, CodeForCall, MustPreserveRegisters);
}

char* JIT_OPERATION operationVirtualConstructThatPreservesRegs(ExecState* execCallee, CallLinkInfo*)
{
    return virtualFor(execCallee, CodeForConstruct, MustPreserveRegisters);
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

size_t JIT_OPERATION operationHasProperty(ExecState* exec, JSObject* base, JSString* property)
{
    int result = base->hasProperty(exec, property->toIdentifier(exec));
    return result;
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

EncodedJSValue JIT_OPERATION operationNewFunction(ExecState* exec, JSScope* scope, JSCell* functionExecutable)
{
    ASSERT(functionExecutable->inherits(FunctionExecutable::info()));
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSValue::encode(JSFunction::create(vm, static_cast<FunctionExecutable*>(functionExecutable), scope));
}

JSCell* JIT_OPERATION operationNewObject(ExecState* exec, Structure* structure)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    return constructEmptyObject(exec, structure);
}

EncodedJSValue JIT_OPERATION operationNewRegexp(ExecState* exec, void* regexpPtr)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    RegExp* regexp = static_cast<RegExp*>(regexpPtr);
    if (!regexp->isValid()) {
        vm.throwException(exec, createSyntaxError(exec, ASCIILiteral("Invalid flags supplied to RegExp constructor.")));
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(RegExpObject::create(vm, exec->lexicalGlobalObject()->regExpStructure(), regexp));
}

void JIT_OPERATION operationHandleWatchdogTimer(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    if (UNLIKELY(vm.watchdog && vm.watchdog->didFire(exec)))
        vm.throwException(exec, createTerminatedExecutionException(&vm));
}

void JIT_OPERATION operationThrowStaticError(ExecState* exec, EncodedJSValue encodedValue, int32_t referenceErrorFlag)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    String message = errorDescriptionForValue(exec, JSValue::decode(encodedValue))->value(exec);
    if (referenceErrorFlag)
        vm.throwException(exec, createReferenceError(exec, message));
    else
        vm.throwException(exec, createTypeError(exec, message));
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
        for (size_t i = 0; i < mustHandleValues.size(); ++i) {
            int operand = mustHandleValues.operandForIndex(i);
            if (operandIsArgument(operand)
                && !VirtualRegister(operand).toArgument()
                && codeBlock->codeType() == FunctionCode
                && codeBlock->specializationKind() == CodeForConstruct) {
                // Ugh. If we're in a constructor, the 'this' argument may hold garbage. It will
                // also never be used. It doesn't matter what we put into the value for this,
                // but it has to be an actual value that can be grokked by subsequent DFG passes,
                // so we sanitize it here by turning it into Undefined.
                mustHandleValues[i] = jsUndefined();
            } else
                mustHandleValues[i] = exec->uncheckedR(operand).jsValue();
        }

        RefPtr<CodeBlock> replacementCodeBlock = codeBlock->newReplacement();
        CompilationResult result = DFG::compile(
            vm, replacementCodeBlock.get(), 0, DFG::DFGMode, bytecodeIndex,
            mustHandleValues, JITToDFGDeferredCompilationCallback::create());
        
        if (result != CompilationSuccessful) {
            ASSERT(result == CompilationDeferred || replacementCodeBlock->hasOneRef());
            return encodeResult(0, 0);
        }
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

#if PLATFORM(IOS) && CPU(ARM_THUMB2)

// The following code is taken from netlib.org:
//   http://www.netlib.org/fdlibm/fdlibm.h
//   http://www.netlib.org/fdlibm/e_pow.c
//   http://www.netlib.org/fdlibm/s_scalbn.c
//
// And was originally distributed under the following license:

/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_pow(x,y) return x**y
 *
 *              n
 * Method:  Let x =  2   * (1+f)
 *    1. Compute and return log2(x) in two pieces:
 *        log2(x) = w1 + w2,
 *       where w1 has 53-24 = 29 bit trailing zeros.
 *    2. Perform y*log2(x) = n+y' by simulating muti-precision 
 *       arithmetic, where |y'|<=0.5.
 *    3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *    1.  (anything) ** 0  is 1
 *    2.  (anything) ** 1  is itself
 *    3.  (anything) ** NAN is NAN
 *    4.  NAN ** (anything except 0) is NAN
 *    5.  +-(|x| > 1) **  +INF is +INF
 *    6.  +-(|x| > 1) **  -INF is +0
 *    7.  +-(|x| < 1) **  +INF is +0
 *    8.  +-(|x| < 1) **  -INF is +INF
 *    9.  +-1         ** +-INF is NAN
 *    10. +0 ** (+anything except 0, NAN)               is +0
 *    11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *    12. +0 ** (-anything except 0, NAN)               is +INF
 *    13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *    14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *    15. +INF ** (+anything except 0,NAN) is +INF
 *    16. +INF ** (-anything except 0,NAN) is +0
 *    17. -INF ** (anything)  = -0 ** (-anything)
 *    18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *    19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *    pow(x,y) returns x**y nearly rounded. In particular
 *            pow(integer,integer)
 *    always returns the correct integer provided it is 
 *    representable.
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough 
 * to produce the hexadecimal values shown.
 */

#define __HI(x) *(1+(int*)&x)
#define __LO(x) *(int*)&x

static const double
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
zero    =  0.0,
one    =  1.0,
two    =  2.0,
two53    =  9007199254740992.0,    /* 0x43400000, 0x00000000 */
huge    =  1.0e300,
tiny    =  1.0e-300,
        /* for scalbn */
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
    /* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
L1  =  5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
L2  =  4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
L3  =  3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
L4  =  2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
L5  =  2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
L6  =  2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
P1   =  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
P2   = -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
P3   =  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
P4   = -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
P5   =  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/

inline double fdlibmScalbn (double x, int n)
{
    int  k,hx,lx;
    hx = __HI(x);
    lx = __LO(x);
        k = (hx&0x7ff00000)>>20;        /* extract exponent */
        if (k==0) {                /* 0 or subnormal x */
            if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
        x *= two54; 
        hx = __HI(x);
        k = ((hx&0x7ff00000)>>20) - 54; 
            if (n< -50000) return tiny*x;     /*underflow*/
        }
        if (k==0x7ff) return x+x;        /* NaN or Inf */
        k = k+n; 
        if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
        if (k > 0)                 /* normal result */
        {__HI(x) = (hx&0x800fffff)|(k<<20); return x;}
        if (k <= -54) {
            if (n > 50000)     /* in case integer overflow in n+k */
        return huge*copysign(huge,x);    /*overflow*/
        else return tiny*copysign(tiny,x);     /*underflow*/
        }
        k += 54;                /* subnormal result */
        __HI(x) = (hx&0x800fffff)|(k<<20);
        return x*twom54;
}

static double fdlibmPow(double x, double y)
{
    double z,ax,z_h,z_l,p_h,p_l;
    double y1,t1,t2,r,s,t,u,v,w;
    int i0,i1,i,j,k,yisint,n;
    int hx,hy,ix,iy;
    unsigned lx,ly;

    i0 = ((*(int*)&one)>>29)^1; i1=1-i0;
    hx = __HI(x); lx = __LO(x);
    hy = __HI(y); ly = __LO(y);
    ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
    if((iy|ly)==0) return one;     

    /* +-NaN return x+y */
    if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
       iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0))) 
        return x+y;    

    /* determine if y is an odd int when x < 0
     * yisint = 0    ... y is not an integer
     * yisint = 1    ... y is an odd int
     * yisint = 2    ... y is an even int
     */
    yisint  = 0;
    if(hx<0) {    
        if(iy>=0x43400000) yisint = 2; /* even integer y */
        else if(iy>=0x3ff00000) {
        k = (iy>>20)-0x3ff;       /* exponent */
        if(k>20) {
            j = ly>>(52-k);
            if(static_cast<unsigned>(j<<(52-k))==ly) yisint = 2-(j&1);
        } else if(ly==0) {
            j = iy>>(20-k);
            if((j<<(20-k))==iy) yisint = 2-(j&1);
        }
        }        
    } 

    /* special value of y */
    if(ly==0) {     
        if (iy==0x7ff00000) {    /* y is +-inf */
            if(((ix-0x3ff00000)|lx)==0)
            return  y - y;    /* inf**+-1 is NaN */
            else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
            return (hy>=0)? y: zero;
            else            /* (|x|<1)**-,+inf = inf,0 */
            return (hy<0)?-y: zero;
        } 
        if(iy==0x3ff00000) {    /* y is  +-1 */
        if(hy<0) return one/x; else return x;
        }
        if(hy==0x40000000) return x*x; /* y is  2 */
        if(hy==0x3fe00000) {    /* y is  0.5 */
        if(hx>=0)    /* x >= +0 */
        return sqrt(x);    
        }
    }

    ax   = fabs(x);
    /* special value of x */
    if(lx==0) {
        if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
        z = ax;            /*x is +-0,+-inf,+-1*/
        if(hy<0) z = one/z;    /* z = (1/|x|) */
        if(hx<0) {
            if(((ix-0x3ff00000)|yisint)==0) {
            z = (z-z)/(z-z); /* (-1)**non-int is NaN */
            } else if(yisint==1) 
            z = -z;        /* (x<0)**odd = -(|x|**odd) */
        }
        return z;
        }
    }
    
    n = (hx>>31)+1;

    /* (x<0)**(non-int) is NaN */
    if((n|yisint)==0) return (x-x)/(x-x);

    s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
    if((n|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

    /* |y| is huge */
    if(iy>0x41e00000) { /* if |y| > 2**31 */
        if(iy>0x43f00000){    /* if |y| > 2**64, must o/uflow */
        if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
        if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
        }
    /* over/underflow if x is not close to one */
        if(ix<0x3fefffff) return (hy<0)? s*huge*huge:s*tiny*tiny;
        if(ix>0x3ff00000) return (hy>0)? s*huge*huge:s*tiny*tiny;
    /* now |1-x| is tiny <= 2**-20, suffice to compute 
       log(x) by x-x^2/2+x^3/3-x^4/4 */
        t = ax-one;        /* t has 20 trailing zeros */
        w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
        u = ivln2_h*t;    /* ivln2_h has 21 sig. bits */
        v = t*ivln2_l-w*ivln2;
        t1 = u+v;
        __LO(t1) = 0;
        t2 = v-(t1-u);
    } else {
        double ss,s2,s_h,s_l,t_h,t_l;
        n = 0;
    /* take care subnormal number */
        if(ix<0x00100000)
        {ax *= two53; n -= 53; ix = __HI(ax); }
        n  += ((ix)>>20)-0x3ff;
        j  = ix&0x000fffff;
    /* determine interval */
        ix = j|0x3ff00000;        /* normalize ix */
        if(j<=0x3988E) k=0;        /* |x|<sqrt(3/2) */
        else if(j<0xBB67A) k=1;    /* |x|<sqrt(3)   */
        else {k=0;n+=1;ix -= 0x00100000;}
        __HI(ax) = ix;

    /* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
        u = ax-bp[k];        /* bp[0]=1.0, bp[1]=1.5 */
        v = one/(ax+bp[k]);
        ss = u*v;
        s_h = ss;
        __LO(s_h) = 0;
    /* t_h=ax+bp[k] High */
        t_h = zero;
        __HI(t_h)=((ix>>1)|0x20000000)+0x00080000+(k<<18); 
        t_l = ax - (t_h-bp[k]);
        s_l = v*((u-s_h*t_h)-s_h*t_l);
    /* compute log(ax) */
        s2 = ss*ss;
        r = s2*s2*(L1+s2*(L2+s2*(L3+s2*(L4+s2*(L5+s2*L6)))));
        r += s_l*(s_h+ss);
        s2  = s_h*s_h;
        t_h = 3.0+s2+r;
        __LO(t_h) = 0;
        t_l = r-((t_h-3.0)-s2);
    /* u+v = ss*(1+...) */
        u = s_h*t_h;
        v = s_l*t_h+t_l*ss;
    /* 2/(3log2)*(ss+...) */
        p_h = u+v;
        __LO(p_h) = 0;
        p_l = v-(p_h-u);
        z_h = cp_h*p_h;        /* cp_h+cp_l = 2/(3*log2) */
        z_l = cp_l*p_h+p_l*cp+dp_l[k];
    /* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
        t = (double)n;
        t1 = (((z_h+z_l)+dp_h[k])+t);
        __LO(t1) = 0;
        t2 = z_l-(((t1-t)-dp_h[k])-z_h);
    }

    /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
    y1  = y;
    __LO(y1) = 0;
    p_l = (y-y1)*t1+y*t2;
    p_h = y1*t1;
    z = p_l+p_h;
    j = __HI(z);
    i = __LO(z);
    if (j>=0x40900000) {                /* z >= 1024 */
        if(((j-0x40900000)|i)!=0)            /* if z > 1024 */
        return s*huge*huge;            /* overflow */
        else {
        if(p_l+ovt>z-p_h) return s*huge*huge;    /* overflow */
        }
    } else if((j&0x7fffffff)>=0x4090cc00 ) {    /* z <= -1075 */
        if(((j-0xc090cc00)|i)!=0)         /* z < -1075 */
        return s*tiny*tiny;        /* underflow */
        else {
        if(p_l<=z-p_h) return s*tiny*tiny;    /* underflow */
        }
    }
    /*
     * compute 2**(p_h+p_l)
     */
    i = j&0x7fffffff;
    k = (i>>20)-0x3ff;
    n = 0;
    if(i>0x3fe00000) {        /* if |z| > 0.5, set n = [z+0.5] */
        n = j+(0x00100000>>(k+1));
        k = ((n&0x7fffffff)>>20)-0x3ff;    /* new k for n */
        t = zero;
        __HI(t) = (n&~(0x000fffff>>k));
        n = ((n&0x000fffff)|0x00100000)>>(20-k);
        if(j<0) n = -n;
        p_h -= t;
    } 
    t = p_l+p_h;
    __LO(t) = 0;
    u = t*lg2_h;
    v = (p_l-(t-p_h))*lg2+t*lg2_l;
    z = u+v;
    w = v-(z-u);
    t  = z*z;
    t1  = z - t*(P1+t*(P2+t*(P3+t*(P4+t*P5))));
    r  = (z*t1)/(t1-two)-(w+z*w);
    z  = one-(r-z);
    j  = __HI(z);
    j += (n<<20);
    if((j>>20)<=0) z = fdlibmScalbn(z,n);    /* subnormal output */
    else __HI(z) += (n<<20);
    return s*z;
}

static ALWAYS_INLINE bool isDenormal(double x)
{
    static const uint64_t signbit = 0x8000000000000000ULL;
    static const uint64_t minNormal = 0x0001000000000000ULL;
    return (bitwise_cast<uint64_t>(x) & ~signbit) - 1 < minNormal - 1;
}

static ALWAYS_INLINE bool isEdgeCase(double x)
{
    static const uint64_t signbit = 0x8000000000000000ULL;
    static const uint64_t infinity = 0x7fffffffffffffffULL;
    return (bitwise_cast<uint64_t>(x) & ~signbit) - 1 >= infinity - 1;
}

static ALWAYS_INLINE double mathPowInternal(double x, double y)
{
    if (!isDenormal(x) && !isDenormal(y)) {
        double libmResult = pow(x, y);
        if (libmResult || isEdgeCase(x) || isEdgeCase(y))
            return libmResult;
    }
    return fdlibmPow(x, y);
}

#else

ALWAYS_INLINE double mathPowInternal(double x, double y)
{
    return pow(x, y);
}

#endif

double JSC_HOST_CALL operationMathPow(double x, double y)
{
    if (std::isnan(y))
        return PNaN;
    if (std::isinf(y) && fabs(x) == 1)
        return PNaN;
    return mathPowInternal(x, y);
}

void JIT_OPERATION operationPutByIndex(ExecState* exec, EncodedJSValue encodedArrayValue, int32_t index, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue arrayValue = JSValue::decode(encodedArrayValue);
    ASSERT(isJSArray(arrayValue));
    asArray(arrayValue)->putDirectIndex(exec, index, JSValue::decode(encodedValue));
}

#if USE(JSVALUE64)
void JIT_OPERATION operationPutGetterSetter(ExecState* exec, EncodedJSValue encodedObjectValue, Identifier* identifier, EncodedJSValue encodedGetterValue, EncodedJSValue encodedSetterValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(JSValue::decode(encodedObjectValue).isObject());
    JSObject* baseObj = asObject(JSValue::decode(encodedObjectValue));

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
    baseObj->putDirectAccessor(exec, *identifier, accessor, Accessor);
}
#else
void JIT_OPERATION operationPutGetterSetter(ExecState* exec, JSCell* object, Identifier* identifier, JSCell* getter, JSCell* setter)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(object && object->isObject());
    JSObject* baseObj = object->getObject();

    GetterSetter* accessor = GetterSetter::create(vm, exec->lexicalGlobalObject());

    ASSERT(!getter || getter->isObject());
    ASSERT(!setter || setter->isObject());
    ASSERT(getter || setter);

    if (getter)
        accessor->setGetter(vm, exec->lexicalGlobalObject(), getter->getObject());
    if (setter)
        accessor->setSetter(vm, exec->lexicalGlobalObject(), setter->getObject());
    baseObj->putDirectAccessor(exec, *identifier, accessor, Accessor);
}
#endif

void JIT_OPERATION operationPushNameScope(ExecState* exec, int32_t dst, Identifier* identifier, EncodedJSValue encodedValue, int32_t attibutes, int32_t type)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    // FIXME: This won't work if this operation is called from the DFG or FTL.
    // This should be changed to pass in the new scope.
    JSScope* currentScope = exec->uncheckedR(dst).Register::scope();
    JSNameScope::Type scopeType = static_cast<JSNameScope::Type>(type);
    JSNameScope* scope = JSNameScope::create(exec, currentScope, *identifier, JSValue::decode(encodedValue), attibutes, scopeType);

    // FIXME: This won't work if this operation is called from the DFG or FTL.
    // This should be changed to return the new scope.
    exec->uncheckedR(dst) = scope;
}

#if USE(JSVALUE32_64)
void JIT_OPERATION operationPushCatchScope(ExecState* exec, int32_t dst, Identifier* identifier, EncodedJSValue encodedValue, int32_t attibutes)
{
    operationPushNameScope(exec, dst, identifier, encodedValue, attibutes, JSNameScope::CatchScope);
}

void JIT_OPERATION operationPushFunctionNameScope(ExecState* exec, int32_t dst, Identifier* identifier, EncodedJSValue encodedValue, int32_t attibutes)
{
    operationPushNameScope(exec, dst, identifier, encodedValue, attibutes, JSNameScope::FunctionNameScope);
}
#endif

void JIT_OPERATION operationPushWithScope(ExecState* exec, int32_t dst, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* o = JSValue::decode(encodedValue).toObject(exec);
    if (vm.exception())
        return;

    // FIXME: This won't work if this operation is called from the DFG or FTL.
    // This should be changed to pass in the old scope and return the new scope.
    JSScope* currentScope = exec->uncheckedR(dst).Register::scope();
    exec->uncheckedR(dst) = JSWithScope::create(exec, o, currentScope);
}

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

EncodedJSValue JIT_OPERATION operationCheckHasInstance(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBaseVal)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseVal = JSValue::decode(encodedBaseVal);

    if (baseVal.isObject()) {
        JSObject* baseObject = asObject(baseVal);
        ASSERT(!baseObject->structure(vm)->typeInfo().implementsDefaultHasInstance());
        if (baseObject->structure(vm)->typeInfo().implementsHasInstance()) {
            bool result = baseObject->methodTable(vm)->customHasInstance(baseObject, exec, value);
            return JSValue::encode(jsBoolean(result));
        }
    }

    vm.throwException(exec, createInvalidParameterError(exec, "instanceof", baseVal));
    return JSValue::encode(JSValue());
}

JSCell* JIT_OPERATION operationCreateActivation(ExecState* exec, JSScope* currentScope, int32_t offset)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSLexicalEnvironment* lexicalEnvironment = JSLexicalEnvironment::create(vm, exec, exec->registers() + offset, currentScope, exec->codeBlock());
    return lexicalEnvironment;
}

// FIXME: This is a temporary thunk for the DFG until we add the lexicalEnvironment operand to the DFG CreateArguments node.
JSCell* JIT_OPERATION operationCreateArgumentsForDFG(ExecState* exec)
{
    JSLexicalEnvironment* lexicalEnvironment = exec->lexicalEnvironmentOrNullptr();
    return operationCreateArguments(exec, lexicalEnvironment);
}
    
JSCell* JIT_OPERATION operationCreateArguments(ExecState* exec, JSLexicalEnvironment* lexicalEnvironment)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    // NB: This needs to be exceedingly careful with top call frame tracking, since it
    // may be called from OSR exit, while the state of the call stack is bizarre.
    Arguments* result = Arguments::create(vm, exec, lexicalEnvironment);
    ASSERT(!vm.exception());
    return result;
}

JSCell* JIT_OPERATION operationCreateArgumentsDuringOSRExit(ExecState* exec)
{
    DeferGCForAWhile(exec->vm().heap);
    JSLexicalEnvironment* lexicalEnvironment = exec->lexicalEnvironmentOrNullptr();
    return operationCreateArguments(exec, lexicalEnvironment);
}

EncodedJSValue JIT_OPERATION operationGetArgumentsLength(ExecState* exec, int32_t argumentsRegister)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    // Here we can assume that the argumernts were created. Because otherwise the JIT code would
    // have not made this call.
    Identifier ident(&vm, "length");
    JSValue baseValue = exec->uncheckedR(argumentsRegister).jsValue();
    PropertySlot slot(baseValue);
    return JSValue::encode(baseValue.get(exec, ident, slot));
}

}

static JSValue getByVal(ExecState* exec, JSValue baseValue, JSValue subscript, ReturnAddressPtr returnAddress)
{
    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        VM& vm = exec->vm();
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            if (AtomicStringImpl* existingAtomicString = asString(subscript)->toExistingAtomicString(exec)) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomicString))
                    return result;
            }
        }
    }

    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i)) {
            ctiPatchCallByReturnAddress(exec->codeBlock(), returnAddress, FunctionPtr(operationGetByValString));
            return asString(baseValue)->getIndex(exec, i);
        }
        return baseValue.get(exec, i);
    }

    PropertyName property = subscript.toPropertyKey(exec);
    return baseValue.get(exec, property);
}

extern "C" {
    
EncodedJSValue JIT_OPERATION operationGetByValGeneric(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);

    JSValue result = getByVal(exec, baseValue, subscript, ReturnAddressPtr(OUR_RETURN_ADDRESS));
    return JSValue::encode(result);
}

EncodedJSValue JIT_OPERATION operationGetByValDefault(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    if (baseValue.isObject() && subscript.isInt32()) {
        // See if it's worth optimizing this at all.
        JSObject* object = asObject(baseValue);
        bool didOptimize = false;

        unsigned bytecodeOffset = exec->locationAsBytecodeOffset();
        ASSERT(bytecodeOffset);
        ByValInfo& byValInfo = exec->codeBlock()->getByValInfo(bytecodeOffset - 1);
        ASSERT(!byValInfo.stubRoutine);
        
        if (hasOptimizableIndexing(object->structure(vm))) {
            // Attempt to optimize.
            JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
            if (arrayMode != byValInfo.arrayMode) {
                JIT::compileGetByVal(&vm, exec->codeBlock(), &byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
                didOptimize = true;
            }
        }
        
        if (!didOptimize) {
            // If we take slow path more than 10 times without patching then make sure we
            // never make that mistake again. Or, if we failed to patch and we have some object
            // that intercepts indexed get, then don't even wait until 10 times. For cases
            // where we see non-index-intercepting objects, this gives 10 iterations worth of
            // opportunity for us to observe that the get_by_val may be polymorphic.
            if (++byValInfo.slowPathCount >= 10
                || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
                // Don't ever try to optimize.
                ctiPatchCallByReturnAddress(exec->codeBlock(), ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationGetByValGeneric));
            }
        }
    }
    
    JSValue result = getByVal(exec, baseValue, subscript, ReturnAddressPtr(OUR_RETURN_ADDRESS));
    return JSValue::encode(result);
}
    
EncodedJSValue JIT_OPERATION operationHasIndexedPropertyDefault(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32());

    JSObject* object = asObject(baseValue);
    bool didOptimize = false;

    unsigned bytecodeOffset = exec->locationAsBytecodeOffset();
    ASSERT(bytecodeOffset);
    ByValInfo& byValInfo = exec->codeBlock()->getByValInfo(bytecodeOffset - 1);
    ASSERT(!byValInfo.stubRoutine);
    
    if (hasOptimizableIndexing(object->structure(vm))) {
        // Attempt to optimize.
        JITArrayMode arrayMode = jitArrayModeForStructure(object->structure(vm));
        if (arrayMode != byValInfo.arrayMode) {
            JIT::compileHasIndexedProperty(&vm, exec->codeBlock(), &byValInfo, ReturnAddressPtr(OUR_RETURN_ADDRESS), arrayMode);
            didOptimize = true;
        }
    }
    
    if (!didOptimize) {
        // If we take slow path more than 10 times without patching then make sure we
        // never make that mistake again. Or, if we failed to patch and we have some object
        // that intercepts indexed get, then don't even wait until 10 times. For cases
        // where we see non-index-intercepting objects, this gives 10 iterations worth of
        // opportunity for us to observe that the get_by_val may be polymorphic.
        if (++byValInfo.slowPathCount >= 10
            || object->structure(vm)->typeInfo().interceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero()) {
            // Don't ever try to optimize.
            ctiPatchCallByReturnAddress(exec->codeBlock(), ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationHasIndexedPropertyGeneric)); 
        }
    }
    
    return JSValue::encode(jsBoolean(object->hasProperty(exec, subscript.asUInt32())));
}
    
EncodedJSValue JIT_OPERATION operationHasIndexedPropertyGeneric(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBase);
    JSValue subscript = JSValue::decode(encodedSubscript);
    
    ASSERT(baseValue.isObject());
    ASSERT(subscript.isUInt32());

    JSObject* object = asObject(baseValue);
    return JSValue::encode(jsBoolean(object->hasProperty(exec, subscript.asUInt32())));
}
    
EncodedJSValue JIT_OPERATION operationGetByValString(ExecState* exec, EncodedJSValue encodedBase, EncodedJSValue encodedSubscript)
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
            if (!isJSString(baseValue))
                ctiPatchCallByReturnAddress(exec->codeBlock(), ReturnAddressPtr(OUR_RETURN_ADDRESS), FunctionPtr(operationGetByValDefault));
        }
    } else {
        PropertyName property = subscript.toPropertyKey(exec);
        result = baseValue.get(exec, property);
    }

    return JSValue::encode(result);
}

void JIT_OPERATION operationTearOffArguments(ExecState* exec, JSCell* argumentsCell, JSCell*)
{
    ASSERT(exec->codeBlock()->usesArguments());
    jsCast<Arguments*>(argumentsCell)->tearOff(exec);
}

EncodedJSValue JIT_OPERATION operationDeleteById(ExecState* exec, EncodedJSValue encodedBase, const Identifier* identifier)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* baseObj = JSValue::decode(encodedBase).toObject(exec);
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
    
    ASSERT(!value.isObject() || !proto.isObject());

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
    return JSValue::encode(JSValue::decode(value).toObject(exec));
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

EncodedJSValue JIT_OPERATION operationResolveScope(ExecState* exec, int32_t scopeReg, int32_t identifierIndex)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    const Identifier& ident = exec->codeBlock()->identifier(identifierIndex);
    JSScope* scope = exec->uncheckedR(scopeReg).Register::scope();
    return JSValue::encode(JSScope::resolve(exec, scope, ident));
}

EncodedJSValue JIT_OPERATION operationGetFromScope(ExecState* exec, Instruction* bytecodePC)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    CodeBlock* codeBlock = exec->codeBlock();
    Instruction* pc = bytecodePC;

    const Identifier& ident = codeBlock->identifier(pc[3].u.operand);
    JSObject* scope = jsCast<JSObject*>(exec->uncheckedR(pc[2].u.operand).jsValue());
    ResolveModeAndType modeAndType(pc[4].u.operand);

    PropertySlot slot(scope);
    if (!scope->getPropertySlot(exec, ident, slot)) {
        if (modeAndType.mode() == ThrowIfNotFound)
            vm.throwException(exec, createUndefinedVariableError(exec, ident));
        return JSValue::encode(jsUndefined());
    }

    // Covers implicit globals. Since they don't exist until they first execute, we didn't know how to cache them at compile time.
    if (slot.isCacheableValue() && slot.slotBase() == scope && scope->structure(vm)->propertyAccessesAreCacheable()) {
        if (modeAndType.type() == GlobalProperty || modeAndType.type() == GlobalPropertyWithVarInjectionChecks) {
            Structure* structure = scope->structure(vm);
            {
                ConcurrentJITLocker locker(codeBlock->m_lock);
                pc[5].u.structure.set(exec->vm(), codeBlock->ownerExecutable(), structure);
                pc[6].u.operand = slot.cachedOffset();
            }
            structure->startWatchingPropertyForReplacements(vm, slot.cachedOffset());
        }
    }

    return JSValue::encode(slot.getValue(exec, ident));
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
    ResolveModeAndType modeAndType = ResolveModeAndType(pc[4].u.operand);
    if (modeAndType.type() == LocalClosureVar) {
        JSLexicalEnvironment* environment = jsCast<JSLexicalEnvironment*>(scope);
        environment->registerAt(pc[6].u.operand).set(vm, environment, value);
        if (VariableWatchpointSet* set = pc[5].u.watchpointSet)
            set->notifyWrite(vm, value, "Executed op_put_scope<LocalClosureVar>");
        return;
    }
    if (modeAndType.mode() == ThrowIfNotFound && !scope->hasProperty(exec, ident)) {
        exec->vm().throwException(exec, createUndefinedVariableError(exec, ident));
        return;
    }

    PutPropertySlot slot(scope, codeBlock->isStrictMode());
    scope->methodTable()->put(scope, exec, ident, value, slot);
    
    if (exec->vm().exception())
        return;

    CommonSlowPaths::tryCachePutToScopeGlobal(exec, codeBlock, pc, scope, modeAndType, slot);
}

void JIT_OPERATION operationThrow(ExecState* exec, EncodedJSValue encodedExceptionValue)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue exceptionValue = JSValue::decode(encodedExceptionValue);
    vm->throwException(exec, exceptionValue);

    // Results stored out-of-band in vm.targetMachinePCForThrow, vm.callFrameForThrow & vm.vmEntryFrameForThrow
    genericUnwind(vm, exec, exceptionValue);
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
    pc[1].u.registerPointer->set(*vm, exec->codeBlock()->globalObject(), value);
}

void JIT_OPERATION lookupExceptionHandler(VM* vm, ExecState* exec)
{
    NativeCallFrameTracer tracer(vm, exec);

    JSValue exceptionValue = vm->exception();
    ASSERT(exceptionValue);
    
    genericUnwind(vm, exec, exceptionValue);
    ASSERT(vm->targetMachinePCForThrow);
}

void JIT_OPERATION lookupExceptionHandlerFromCallerFrame(VM* vm, ExecState* exec)
{
    VMEntryFrame* vmEntryFrame = vm->topVMEntryFrame;
    CallFrame* callerFrame = exec->callerFrame(vmEntryFrame);
    ASSERT(callerFrame);

    NativeCallFrameTracerWithRestore tracer(vm, vmEntryFrame, callerFrame);

    JSValue exceptionValue = vm->exception();
    ASSERT(exceptionValue);
    
    genericUnwind(vm, callerFrame, exceptionValue);
    ASSERT(vm->targetMachinePCForThrow);
}

void JIT_OPERATION operationVMHandleException(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    genericUnwind(vm, exec, vm->exception());
}

// This function "should" just take the ExecState*, but doing so would make it more difficult
// to call from exception check sites. So, unlike all of our other functions, we allow
// ourselves to play some gnarly ABI tricks just to simplify the calling convention. This is
// particularly safe here since this is never called on the critical path - it's only for
// testing.
void JIT_OPERATION operationExceptionFuzz()
{
    // This probably "just works" for GCC also, but I haven't tried.
#if COMPILER(CLANG)
    ExecState* exec = static_cast<ExecState*>(__builtin_frame_address(1));
    void* returnPC = __builtin_return_address(0);
    doExceptionFuzzing(exec, "JITOperations", returnPC);
#endif // COMPILER(CLANG)
}

int32_t JIT_OPERATION operationGetEnumerableLength(ExecState* exec, JSCell* baseCell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSObject* base = baseCell->toObject(exec, exec->lexicalGlobalObject());
    return base->methodTable(vm)->getEnumerableLength(exec, base);
}

EncodedJSValue JIT_OPERATION operationHasGenericProperty(ExecState* exec, EncodedJSValue encodedBaseValue, JSCell* propertyName)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSValue baseValue = JSValue::decode(encodedBaseValue);
    if (baseValue.isUndefinedOrNull())
        return JSValue::encode(jsBoolean(false));

    JSObject* base = baseValue.toObject(exec);
    return JSValue::encode(jsBoolean(base->hasProperty(exec, asString(propertyName)->toIdentifier(exec))));
}

EncodedJSValue JIT_OPERATION operationHasIndexedProperty(ExecState* exec, JSCell* baseCell, int32_t subscript)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSObject* object = baseCell->toObject(exec, exec->lexicalGlobalObject());
    return JSValue::encode(jsBoolean(object->hasProperty(exec, subscript)));
}
    
JSCell* JIT_OPERATION operationGetStructurePropertyEnumerator(ExecState* exec, JSCell* cell, int32_t length)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
        
    JSObject* base = cell->toObject(exec, exec->lexicalGlobalObject());
    ASSERT(length >= 0);

    return structurePropertyNameEnumerator(exec, base, static_cast<uint32_t>(length));
}

JSCell* JIT_OPERATION operationGetGenericPropertyEnumerator(ExecState* exec, JSCell* baseCell, int32_t length, JSCell* structureEnumeratorCell)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    
    JSObject* base = baseCell->toObject(exec, exec->lexicalGlobalObject());
    ASSERT(length >= 0);

    return genericPropertyNameEnumerator(exec, base, length, jsCast<JSPropertyNameEnumerator*>(structureEnumeratorCell));
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

#if COMPILER(GCC) && CPU(X86_64)
asm (
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov %rbp, %rdi\n"
    "jmp " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(X86)
asm (
".text" "\n" \
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "push %ebp\n"
    "leal -4(%esp), %esp\n"
    "push %ebp\n"
    "call " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
    "leal 8(%esp), %esp\n"
    "pop %ebp\n"
    "ret\n"
);

#elif COMPILER(GCC) && CPU(ARM_THUMB2)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
".thumb" "\n"
".thumb_func " THUMB_FUNC_PARAM(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov r0, r7" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(ARM_TRADITIONAL)
asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
INLINE_ARM_FUNCTION(getHostCallReturnValue)
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov r0, r11" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif CPU(ARM64)
asm (
".text" "\n"
".align 2" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
     "mov x0, x29" "\n"
     "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(MIPS)

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
    "move $a0, $fp" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(SH4)

#define SH4_SCRATCH_REGISTER "r11"

asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov r14, r4" "\n"
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
        __asm mov [esp + 4], ebp;
        __asm jmp getHostCallReturnValueWithExecState
    }
}
#endif

} // namespace JSC

#endif // ENABLE(JIT)
