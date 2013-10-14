/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#if ENABLE(JIT)
#include "JITOperations.h"

#include "Arguments.h"
#include "ArrayConstructor.h"
#include "CommonSlowPaths.h"
#include "Error.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "JITOperationWrappers.h"
#include "JSGlobalObjectFunctions.h"
#include "JSPropertyNameIterator.h"
#include "ObjectConstructor.h"
#include "Operations.h"
#include "Repatch.h"

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


void JIT_OPERATION operationStackCheck(ExecState* exec, CodeBlock* codeBlock)
{
    // We pass in our own code block, because the callframe hasn't been populated.
    VM* vm = codeBlock->vm();
    CallFrame* callerFrame = exec->callerFrame();
    NativeCallFrameTracer tracer(vm, callerFrame->removeHostCallFrameFlag());

    JSStack& stack = vm->interpreter->stack();

    if (UNLIKELY(!stack.grow(&exec->registers()[-codeBlock->m_numCalleeRegisters])))
        vm->throwException(callerFrame, createStackOverflowError(callerFrame));
}

int32_t JIT_OPERATION operationCallArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    CallFrame* callerFrame = exec->callerFrame();
    NativeCallFrameTracer tracer(vm, callerFrame->removeHostCallFrameFlag());

    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForCall);
    if (missingArgCount < 0)
        vm->throwException(callerFrame, createStackOverflowError(callerFrame));

    return missingArgCount;
}

int32_t JIT_OPERATION operationConstructArityCheck(ExecState* exec)
{
    VM* vm = &exec->vm();
    CallFrame* callerFrame = exec->callerFrame();
    NativeCallFrameTracer tracer(vm, callerFrame->removeHostCallFrameFlag());

    JSStack& stack = vm->interpreter->stack();

    int32_t missingArgCount = CommonSlowPaths::arityCheckFor(exec, &stack, CodeForConstruct);
    if (missingArgCount < 0)
        vm->throwException(callerFrame, createStackOverflowError(callerFrame));

    return missingArgCount;
}

EncodedJSValue JIT_OPERATION operationGetById(ExecState* exec, EncodedJSValue base, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    Identifier ident(vm, uid);
    return JSValue::encode(baseValue.get(exec, ident, slot));
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(operationGetByIdBuildList);
EncodedJSValue JIT_OPERATION operationGetByIdBuildListWithReturnAddress(ExecState* exec, EncodedJSValue base, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, ident, slot);

    if (accessType == static_cast<AccessType>(stubInfo.accessType))
        buildGetByIDList(exec, baseValue, ident, slot, stubInfo);

    return JSValue::encode(result);
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJI(operationGetByIdOptimize);
EncodedJSValue JIT_OPERATION operationGetByIdOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue base, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    Identifier ident = uid->isEmptyUnique() ? Identifier::from(PrivateName(uid)) : Identifier(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue baseValue = JSValue::decode(base);
    PropertySlot slot(baseValue);
    JSValue result = baseValue.get(exec, ident, slot);
    
    if (accessType == static_cast<AccessType>(stubInfo.accessType)) {
        if (stubInfo.seen)
            repatchGetByID(exec, baseValue, ident, slot, stubInfo);
        else
            stubInfo.seen = true;
    }

    return JSValue::encode(result);
}

J_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_ECI(operationInOptimize);
EncodedJSValue JIT_OPERATION operationInOptimizeWithReturnAddress(ExecState* exec, JSCell* base, StringImpl* key, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    if (!base->isObject()) {
        vm->throwException(exec, createInvalidParameterError(exec, "in", base));
        return JSValue::encode(jsUndefined());
    }
    
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    Identifier ident(vm, key);
    PropertySlot slot(base);
    bool result = asObject(base)->getPropertySlot(exec, ident, slot);
    
    RELEASE_ASSERT(accessType == stubInfo.accessType);
    
    if (stubInfo.seen)
        repatchIn(exec, base, ident, result, slot, stubInfo);
    else
        stubInfo.seen = true;
    
    return JSValue::encode(jsBoolean(result));
}

EncodedJSValue JIT_OPERATION operationIn(ExecState* exec, JSCell* base, StringImpl* key)
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

EncodedJSValue JIT_OPERATION operationCallCustomGetter(ExecState* exec, JSCell* base, PropertySlot::GetValueFunc function, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    
    return JSValue::encode(function(exec, asObject(base), ident));
}

EncodedJSValue JIT_OPERATION operationCallGetter(ExecState* exec, JSCell* base, JSCell* getterSetter)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    return JSValue::encode(callGetter(exec, base, getterSetter));
}

void JIT_OPERATION operationPutByIdStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).put(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    JSValue::decode(encodedBase).put(exec, ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

void JIT_OPERATION operationPutByIdDirectNonStrict(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    asObject(JSValue::decode(encodedBase))->putDirect(exec->vm(), ident, JSValue::decode(encodedValue), slot);
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdStrictOptimize);
void JIT_OPERATION operationPutByIdStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    if (stubInfo.seen)
        repatchPutByID(exec, baseValue, ident, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdNonStrictOptimize);
void JIT_OPERATION operationPutByIdNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    if (stubInfo.seen)
        repatchPutByID(exec, baseValue, ident, slot, stubInfo, NotDirect);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdDirectStrictOptimize);
void JIT_OPERATION operationPutByIdDirectStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    if (stubInfo.seen)
        repatchPutByID(exec, baseObject, ident, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdDirectNonStrictOptimize);
void JIT_OPERATION operationPutByIdDirectNonStrictOptimizeWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    if (stubInfo.seen)
        repatchPutByID(exec, baseObject, ident, slot, stubInfo, Direct);
    else
        stubInfo.seen = true;
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdStrictBuildList);
void JIT_OPERATION operationPutByIdStrictBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    buildPutByIdList(exec, baseValue, ident, slot, stubInfo, NotDirect);
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdNonStrictBuildList);
void JIT_OPERATION operationPutByIdNonStrictBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseValue = JSValue::decode(encodedBase);
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    
    baseValue.put(exec, ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    buildPutByIdList(exec, baseValue, ident, slot, stubInfo, NotDirect);
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdDirectStrictBuildList);
void JIT_OPERATION operationPutByIdDirectStrictBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);
    
    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(true, exec->codeBlock()->putByIdContext());
    
    baseObject->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    buildPutByIdList(exec, baseObject, ident, slot, stubInfo, Direct);
}

V_FUNCTION_WRAPPER_WITH_RETURN_ADDRESS_EJJI(operationPutByIdDirectNonStrictBuildList);
void JIT_OPERATION operationPutByIdDirectNonStrictBuildListWithReturnAddress(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBase, StringImpl* uid, ReturnAddressPtr returnAddress)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    Identifier ident(vm, uid);
    StructureStubInfo& stubInfo = exec->codeBlock()->getStubInfo(returnAddress);
    AccessType accessType = static_cast<AccessType>(stubInfo.accessType);

    JSValue value = JSValue::decode(encodedValue);
    JSObject* baseObject = asObject(JSValue::decode(encodedBase));
    PutPropertySlot slot(false, exec->codeBlock()->putByIdContext());
    
    baseObject ->putDirect(exec->vm(), ident, value, slot);
    
    if (accessType != static_cast<AccessType>(stubInfo.accessType))
        return;
    
    buildPutByIdList(exec, baseObject, ident, slot, stubInfo, Direct);
}

void JIT_OPERATION operationReallocateStorageAndFinishPut(ExecState* exec, JSObject* base, Structure* structure, PropertyOffset offset, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    ASSERT(structure->outOfLineCapacity() > base->structure()->outOfLineCapacity());
    ASSERT(!vm.heap.storageAllocator().fastPathShouldSucceed(structure->outOfLineCapacity() * sizeof(JSValue)));
    base->setStructureAndReallocateStorageIfNecessary(vm, structure);
    base->putDirect(vm, offset, JSValue::decode(value));
}

EncodedJSValue JIT_OPERATION operationCallEval(ExecState* execCallee)
{
    CallFrame* callerFrame = execCallee->callerFrame();
    ASSERT(execCallee->callerFrame()->codeBlock()->codeType() != FunctionCode
        || !execCallee->callerFrame()->codeBlock()->needsFullScopeChain()
        || execCallee->callerFrame()->uncheckedR(execCallee->callerFrame()->codeBlock()->activationRegister().offset()).jsValue());

    execCallee->setScope(callerFrame->scope());
    execCallee->setReturnPC(static_cast<Instruction*>(OUR_RETURN_ADDRESS));
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

    execCallee->setScope(exec->scope());
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

inline char* linkFor(ExecState* execCallee, CodeSpecializationKind kind)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);
    
    JSValue calleeAsValue = execCallee->calleeAsValue();
    JSCell* calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (!calleeAsFunctionCell)
        return reinterpret_cast<char*>(handleHostCall(execCallee, calleeAsValue, kind));

    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    execCallee->setScope(callee->scopeUnchecked());
    ExecutableBase* executable = callee->executable();

    MacroAssemblerCodePtr codePtr;
    CodeBlock* codeBlock = 0;
    CallLinkInfo& callLinkInfo = exec->codeBlock()->getCallLinkInfo(execCallee->returnPC());
    if (executable->isHostFunction())
        codePtr = executable->generatedJITCodeFor(kind)->addressForCall();
    else {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->prepareForExecution(execCallee, callee->scope(), kind);
        if (error) {
            vm->throwException(exec, createStackOverflowError(exec));
            return reinterpret_cast<char*>(vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress());
        }
        codeBlock = functionExecutable->codeBlockFor(kind);
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()) || callLinkInfo.callType == CallLinkInfo::CallVarargs)
            codePtr = functionExecutable->generatedJITCodeWithArityCheckFor(kind);
        else
            codePtr = functionExecutable->generatedJITCodeFor(kind)->addressForCall();
    }
    if (!callLinkInfo.seenOnce())
        callLinkInfo.setSeen();
    else
        linkFor(execCallee, callLinkInfo, codeBlock, callee, codePtr, kind);
    return reinterpret_cast<char*>(codePtr.executableAddress());
}

char* JIT_OPERATION operationLinkCall(ExecState* execCallee)
{
    return linkFor(execCallee, CodeForCall);
}

char* JIT_OPERATION operationLinkConstruct(ExecState* execCallee)
{
    return linkFor(execCallee, CodeForConstruct);
}

inline char* virtualForWithFunction(ExecState* execCallee, CodeSpecializationKind kind, JSCell*& calleeAsFunctionCell)
{
    ExecState* exec = execCallee->callerFrame();
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue calleeAsValue = execCallee->calleeAsValue();
    calleeAsFunctionCell = getJSFunction(calleeAsValue);
    if (UNLIKELY(!calleeAsFunctionCell))
        return reinterpret_cast<char*>(handleHostCall(execCallee, calleeAsValue, kind));
    
    JSFunction* function = jsCast<JSFunction*>(calleeAsFunctionCell);
    execCallee->setScope(function->scopeUnchecked());
    ExecutableBase* executable = function->executable();
    if (UNLIKELY(!executable->hasJITCodeFor(kind))) {
        FunctionExecutable* functionExecutable = static_cast<FunctionExecutable*>(executable);
        JSObject* error = functionExecutable->prepareForExecution(execCallee, function->scope(), kind);
        if (error) {
            exec->vm().throwException(execCallee, error);
            return reinterpret_cast<char*>(vm->getCTIStub(throwExceptionFromCallSlowPathGenerator).code().executableAddress());
        }
    }
    return reinterpret_cast<char*>(executable->generatedJITCodeWithArityCheckFor(kind).executableAddress());
}

inline char* virtualFor(ExecState* execCallee, CodeSpecializationKind kind)
{
    JSCell* calleeAsFunctionCellIgnored;
    return virtualForWithFunction(execCallee, kind, calleeAsFunctionCellIgnored);
}

static bool attemptToOptimizeClosureCall(ExecState* execCallee, JSCell* calleeAsFunctionCell, CallLinkInfo& callLinkInfo)
{
    if (!calleeAsFunctionCell)
        return false;
    
    JSFunction* callee = jsCast<JSFunction*>(calleeAsFunctionCell);
    JSFunction* oldCallee = callLinkInfo.callee.get();
    
    if (!oldCallee
        || oldCallee->structure() != callee->structure()
        || oldCallee->executable() != callee->executable())
        return false;
    
    ASSERT(callee->executable()->hasJITCodeForCall());
    MacroAssemblerCodePtr codePtr = callee->executable()->generatedJITCodeForCall()->addressForCall();
    
    CodeBlock* codeBlock;
    if (callee->executable()->isHostFunction())
        codeBlock = 0;
    else {
        codeBlock = jsCast<FunctionExecutable*>(callee->executable())->codeBlockForCall();
        if (execCallee->argumentCountIncludingThis() < static_cast<size_t>(codeBlock->numParameters()))
            return false;
    }
    
    linkClosureCall(
        execCallee, callLinkInfo, codeBlock,
        callee->structure(), callee->executable(), codePtr);
    
    return true;
}

char* JIT_OPERATION operationLinkClosureCall(ExecState* execCallee)
{
    JSCell* calleeAsFunctionCell;
    char* result = virtualForWithFunction(execCallee, CodeForCall, calleeAsFunctionCell);
    CallLinkInfo& callLinkInfo = execCallee->callerFrame()->codeBlock()->getCallLinkInfo(execCallee->returnPC());

    if (!attemptToOptimizeClosureCall(execCallee, calleeAsFunctionCell, callLinkInfo))
        linkSlowFor(execCallee, callLinkInfo, CodeForCall);
    
    return result;
}

char* JIT_OPERATION operationVirtualCall(ExecState* execCallee)
{    
    return virtualFor(execCallee, CodeForCall);
}

char* JIT_OPERATION operationVirtualConstruct(ExecState* execCallee)
{
    return virtualFor(execCallee, CodeForConstruct);
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

    bool result = asString(left)->value(exec) == asString(right)->value(exec);
#if USE(JSVALUE64)
    return JSValue::encode(jsBoolean(result));
#else
    return result;
#endif
}

size_t JIT_OPERATION operationHasProperty(ExecState* exec, JSObject* base, JSString* property)
{
    int result = base->hasProperty(exec, Identifier(exec, property->value(exec)));
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

EncodedJSValue JIT_OPERATION operationNewFunction(ExecState* exec, JSCell* functionExecutable)
{
    ASSERT(functionExecutable->inherits(FunctionExecutable::info()));
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSValue::encode(JSFunction::create(vm, static_cast<FunctionExecutable*>(functionExecutable), exec->scope()));
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
        vm.throwException(exec, createSyntaxError(exec, "Invalid flags supplied to RegExp constructor."));
        return JSValue::encode(jsUndefined());
    }

    return JSValue::encode(RegExpObject::create(vm, exec->lexicalGlobalObject()->regExpStructure(), regexp));
}

EncodedJSValue JIT_OPERATION operationCheckHasInstance(ExecState* exec, EncodedJSValue encodedValue, EncodedJSValue encodedBaseVal)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue value = JSValue::decode(encodedValue);
    JSValue baseVal = JSValue::decode(encodedBaseVal);

    if (baseVal.isObject()) {
        JSObject* baseObject = asObject(baseVal);
        ASSERT(!baseObject->structure()->typeInfo().implementsDefaultHasInstance());
        if (baseObject->structure()->typeInfo().implementsHasInstance()) {
            bool result = baseObject->methodTable()->customHasInstance(baseObject, exec, value);
            return JSValue::encode(jsBoolean(result));
        }
    }

    vm->throwException(exec, createInvalidParameterError(exec, "instanceof", baseVal));
    return JSValue::encode(JSValue());
}

JSCell* JIT_OPERATION operationCreateActivation(ExecState* exec, int32_t offset)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSActivation* activation = JSActivation::create(vm, exec, exec->registers() + offset, exec->codeBlock());
    exec->setScope(activation);
    return activation;
}

JSCell* JIT_OPERATION operationCreateArguments(ExecState* exec)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    // NB: This needs to be exceedingly careful with top call frame tracking, since it
    // may be called from OSR exit, while the state of the call stack is bizarre.
    Arguments* result = Arguments::create(vm, exec);
    ASSERT(!vm.exception());
    return result;
}

EncodedJSValue JIT_OPERATION operationDeleteById(ExecState* exec, EncodedJSValue encodedBase, const Identifier* identifier)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    JSObject* baseObj = JSValue::decode(encodedBase).toObject(exec);
    bool couldDelete = baseObj->methodTable()->deleteProperty(baseObj, exec, *identifier);
    JSValue result = jsBoolean(couldDelete);
    if (!couldDelete && exec->codeBlock()->isStrictMode())
        vm.throwException(exec, createTypeError(exec, "Unable to delete property."));
    return JSValue::encode(result);
}

JSCell* JIT_OPERATION operationGetPNames(ExecState* exec, JSObject* obj)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);

    Structure* structure = obj->structure();
    JSPropertyNameIterator* jsPropertyNameIterator = structure->enumerationCache();
    if (!jsPropertyNameIterator || jsPropertyNameIterator->cachedPrototypeChain() != structure->prototypeChain(exec))
        jsPropertyNameIterator = JSPropertyNameIterator::create(exec, obj);
    return jsPropertyNameIterator;
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

CallFrame* JIT_OPERATION operationLoadVarargs(ExecState* exec, EncodedJSValue encodedThis, EncodedJSValue encodedArguments, int32_t firstFreeRegister)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    JSStack* stack = &exec->interpreter()->stack();
    JSValue thisValue = JSValue::decode(encodedThis);
    JSValue arguments = JSValue::decode(encodedArguments);
    CallFrame* newCallFrame = loadVarargs(exec, stack, thisValue, arguments, firstFreeRegister);
    return newCallFrame;
}

EncodedJSValue JIT_OPERATION operationToObject(ExecState* exec, EncodedJSValue value)
{
    VM& vm = exec->vm();
    NativeCallFrameTracer tracer(&vm, exec);
    return JSValue::encode(JSValue::decode(value).toObject(exec));
}

JITHandlerEncoded JIT_OPERATION lookupExceptionHandler(ExecState* exec)
{
    VM* vm = &exec->vm();
    NativeCallFrameTracer tracer(vm, exec);

    JSValue exceptionValue = exec->exception();
    ASSERT(exceptionValue);
    
    ExceptionHandler handler = genericUnwind(vm, exec, exceptionValue);
    ASSERT(handler.catchRoutine);
    return dfgHandlerEncoded(handler.callFrame, handler.catchRoutine);
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
    "mov 40(%r13), %r13\n"
    "mov %r13, %rdi\n"
    "jmp " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(X86)
asm (
".text" "\n" \
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "mov 40(%edi), %edi\n"
    "mov %edi, 4(%esp)\n"
    "jmp " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
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
    "ldr r5, [r5, #40]" "\n"
    "mov r0, r5" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(ARM_TRADITIONAL)
asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
INLINE_ARM_FUNCTION(getHostCallReturnValue)
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "ldr r5, [r5, #40]" "\n"
    "mov r0, r5" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(MIPS)
asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    LOAD_FUNCTION_TO_T9(getHostCallReturnValueWithExecState)
    "lw $s0, 40($s0)" "\n"
    "move $a0, $s0" "\n"
    "b " LOCAL_REFERENCE(getHostCallReturnValueWithExecState) "\n"
);

#elif COMPILER(GCC) && CPU(SH4)
asm (
".text" "\n"
".globl " SYMBOL_STRING(getHostCallReturnValue) "\n"
HIDE_SYMBOL(getHostCallReturnValue) "\n"
SYMBOL_STRING(getHostCallReturnValue) ":" "\n"
    "add #40, r14" "\n"
    "mov.l @r14, r14" "\n"
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
        __asm {
            mov edi, [edi + 40];
            mov [esp + 4], edi;
            jmp getHostCallReturnValueWithExecState
        }
    }
}
#endif

} // namespace JSC

#endif // ENABLE(JIT)
