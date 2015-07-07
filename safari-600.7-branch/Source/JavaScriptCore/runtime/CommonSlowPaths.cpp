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
#include "CommonSlowPaths.h"
#include "Arguments.h"
#include "ArityCheckFailReturnThunks.h"
#include "ArrayConstructor.h"
#include "CallFrame.h"
#include "CodeProfiling.h"
#include "CommonSlowPathsExceptions.h"
#include "ErrorHandlingScope.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JITStubs.h"
#include "JSActivation.h"
#include "JSCJSValue.h"
#include "JSGlobalObjectFunctions.h"
#include "JSNameScope.h"
#include "JSPropertyNameIterator.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCommon.h"
#include "LLIntExceptions.h"
#include "LowLevelInterpreter.h"
#include "ObjectConstructor.h"
#include "JSCInlines.h"
#include "StructureRareDataInlines.h"
#include "VariableWatchpointSetInlines.h"
#include <wtf/StringPrintStream.h>

namespace JSC {

#define BEGIN_NO_SET_PC() \
    VM& vm = exec->vm();      \
    NativeCallFrameTracer tracer(&vm, exec)

#ifndef NDEBUG
#define SET_PC_FOR_STUBS() do { \
        exec->codeBlock()->bytecodeOffset(pc); \
        exec->setCurrentVPC(pc + 1); \
    } while (false)
#else
#define SET_PC_FOR_STUBS() do { \
        exec->setCurrentVPC(pc + 1); \
    } while (false)
#endif

#define RETURN_TO_THROW(exec, pc)   pc = LLInt::returnToThrow(exec)

#define BEGIN()                           \
    BEGIN_NO_SET_PC();                    \
    SET_PC_FOR_STUBS()

#define OP(index) (exec->uncheckedR(pc[index].u.operand))
#define OP_C(index) (exec->r(pc[index].u.operand))

#define RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define END_IMPL() RETURN_TWO(pc, exec)

#define THROW(exceptionToThrow) do {                        \
        vm.throwException(exec, exceptionToThrow);          \
        RETURN_TO_THROW(exec, pc);                          \
        END_IMPL();                                         \
    } while (false)

#define CHECK_EXCEPTION() do {                    \
        if (UNLIKELY(vm.exception())) {           \
            RETURN_TO_THROW(exec, pc);               \
            END_IMPL();                           \
        }                                               \
    } while (false)

#define END() do {                        \
        CHECK_EXCEPTION();                \
        END_IMPL();                       \
    } while (false)

#define BRANCH(opcode, condition) do {                      \
        bool bCondition = (condition);                         \
        CHECK_EXCEPTION();                                  \
        if (bCondition)                                        \
            pc += pc[OPCODE_LENGTH(opcode) - 1].u.operand;        \
        else                                                      \
            pc += OPCODE_LENGTH(opcode);                          \
        END_IMPL();                                         \
    } while (false)

#define RETURN(value) do {                \
        JSValue rReturnValue = (value);      \
        CHECK_EXCEPTION();                \
        OP(1) = rReturnValue;          \
        END_IMPL();                       \
    } while (false)

#define RETURN_PROFILED(opcode, value) do {                  \
        JSValue rpPeturnValue = (value);                     \
        CHECK_EXCEPTION();                                   \
        OP(1) = rpPeturnValue;                               \
        PROFILE_VALUE(opcode, rpPeturnValue);                \
        END_IMPL();                                          \
    } while (false)

#define PROFILE_VALUE(opcode, value) do { \
        pc[OPCODE_LENGTH(opcode) - 1].u.profile->m_buckets[0] = \
        JSValue::encode(value);                  \
    } while (false)

#define CALL_END_IMPL(exec, callTarget) RETURN_TWO((callTarget), (exec))

#define CALL_THROW(exec, pc, exceptionToThrow) do {                     \
        ExecState* ctExec = (exec);                                     \
        Instruction* ctPC = (pc);                                       \
        vm.throwException(exec, exceptionToThrow);                      \
        CALL_END_IMPL(ctExec, LLInt::callToThrow(ctExec));              \
    } while (false)

#define CALL_CHECK_EXCEPTION(exec, pc) do {                          \
        ExecState* cceExec = (exec);                                 \
        Instruction* ccePC = (pc);                                   \
        if (UNLIKELY(vm.exception()))                                \
            CALL_END_IMPL(cceExec, LLInt::callToThrow(cceExec));     \
    } while (false)

#define CALL_RETURN(exec, pc, callTarget) do {                    \
        ExecState* crExec = (exec);                                  \
        Instruction* crPC = (pc);                                    \
        void* crCallTarget = (callTarget);                           \
        CALL_CHECK_EXCEPTION(crExec->callerFrame(), crPC);  \
        CALL_END_IMPL(crExec, crCallTarget);                \
    } while (false)

static CommonSlowPaths::ArityCheckData* setupArityCheckData(VM& vm, int slotsToAdd)
{
    CommonSlowPaths::ArityCheckData* result = vm.arityCheckData.get();
    result->paddedStackSpace = slotsToAdd;
#if ENABLE(JIT)
    if (vm.canUseJIT()) {
        result->thunkToCall = vm.getCTIStub(arityFixup).code().executableAddress();
        result->returnPC = vm.arityCheckFailReturnThunks->returnPCFor(vm, slotsToAdd * stackAlignmentRegisters()).executableAddress();
    } else
#endif
    {
        result->thunkToCall = 0;
        result->returnPC = 0;
    }
    return result;
}

SLOW_PATH_DECL(slow_path_call_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(exec, &vm.interpreter->stack(), CodeForCall);
    if (slotsToAdd < 0) {
        exec = exec->callerFrame();
        ErrorHandlingScope errorScope(exec->vm());
        CommonSlowPaths::interpreterThrowInCaller(exec, createStackOverflowError(exec));
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    RETURN_TWO(0, setupArityCheckData(vm, slotsToAdd));
}

SLOW_PATH_DECL(slow_path_construct_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(exec, &vm.interpreter->stack(), CodeForConstruct);
    if (slotsToAdd < 0) {
        exec = exec->callerFrame();
        ErrorHandlingScope errorScope(exec->vm());
        CommonSlowPaths::interpreterThrowInCaller(exec, createStackOverflowError(exec));
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    RETURN_TWO(0, setupArityCheckData(vm, slotsToAdd));
}

SLOW_PATH_DECL(slow_path_touch_entry)
{
    BEGIN();
    exec->codeBlock()->symbolTable()->m_functionEnteredOnce.touch();
    END();
}

SLOW_PATH_DECL(slow_path_get_callee)
{
    BEGIN();
    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    pc[2].u.jsCell.set(exec->vm(), exec->codeBlock()->ownerExecutable(), callee);
    RETURN(callee);
}

SLOW_PATH_DECL(slow_path_create_arguments)
{
    BEGIN();
    JSValue arguments = JSValue(Arguments::create(vm, exec));
    CHECK_EXCEPTION();
    exec->uncheckedR(pc[1].u.operand) = arguments;
    exec->uncheckedR(unmodifiedArgumentsRegister(VirtualRegister(pc[1].u.operand)).offset()) = arguments;
    END();
}

SLOW_PATH_DECL(slow_path_create_this)
{
    BEGIN();
    JSFunction* constructor = jsCast<JSFunction*>(OP(2).jsValue().asCell());
    
#if !ASSERT_DISABLED
    ConstructData constructData;
    ASSERT(constructor->methodTable()->getConstructData(constructor, constructData) == ConstructTypeJS);
#endif

    size_t inlineCapacity = pc[3].u.operand;
    Structure* structure = constructor->allocationProfile(exec, inlineCapacity)->structure();
    RETURN(constructEmptyObject(exec, structure));
}

SLOW_PATH_DECL(slow_path_to_this)
{
    BEGIN();
    JSValue v1 = OP(1).jsValue();
    if (v1.isCell())
        pc[2].u.structure.set(vm, exec->codeBlock()->ownerExecutable(), v1.asCell()->structure(vm));
    else
        pc[2].u.structure.clear();
    RETURN(v1.toThis(exec, exec->codeBlock()->isStrictMode() ? StrictMode : NotStrictMode));
}

SLOW_PATH_DECL(slow_path_captured_mov)
{
    BEGIN();
    JSValue value = OP_C(2).jsValue();
    if (VariableWatchpointSet* set = pc[3].u.watchpointSet)
        set->notifyWrite(vm, value);
    RETURN(value);
}

SLOW_PATH_DECL(slow_path_new_captured_func)
{
    BEGIN();
    CodeBlock* codeBlock = exec->codeBlock();
    ASSERT(codeBlock->codeType() != FunctionCode || !codeBlock->needsActivation() || exec->hasActivation());
    JSValue value = JSFunction::create(vm, codeBlock->functionDecl(pc[2].u.operand), exec->scope());
    if (VariableWatchpointSet* set = pc[3].u.watchpointSet)
        set->notifyWrite(vm, value);
    RETURN(value);
}

SLOW_PATH_DECL(slow_path_not)
{
    BEGIN();
    RETURN(jsBoolean(!OP_C(2).jsValue().toBoolean(exec)));
}

SLOW_PATH_DECL(slow_path_eq)
{
    BEGIN();
    RETURN(jsBoolean(JSValue::equal(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_neq)
{
    BEGIN();
    RETURN(jsBoolean(!JSValue::equal(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_stricteq)
{
    BEGIN();
    RETURN(jsBoolean(JSValue::strictEqual(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_nstricteq)
{
    BEGIN();
    RETURN(jsBoolean(!JSValue::strictEqual(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_less)
{
    BEGIN();
    RETURN(jsBoolean(jsLess<true>(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_lesseq)
{
    BEGIN();
    RETURN(jsBoolean(jsLessEq<true>(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_greater)
{
    BEGIN();
    RETURN(jsBoolean(jsLess<false>(exec, OP_C(3).jsValue(), OP_C(2).jsValue())));
}

SLOW_PATH_DECL(slow_path_greatereq)
{
    BEGIN();
    RETURN(jsBoolean(jsLessEq<false>(exec, OP_C(3).jsValue(), OP_C(2).jsValue())));
}

SLOW_PATH_DECL(slow_path_inc)
{
    BEGIN();
    RETURN(jsNumber(OP(1).jsValue().toNumber(exec) + 1));
}

SLOW_PATH_DECL(slow_path_dec)
{
    BEGIN();
    RETURN(jsNumber(OP(1).jsValue().toNumber(exec) - 1));
}

SLOW_PATH_DECL(slow_path_to_number)
{
    BEGIN();
    RETURN(jsNumber(OP_C(2).jsValue().toNumber(exec)));
}

SLOW_PATH_DECL(slow_path_negate)
{
    BEGIN();
    RETURN(jsNumber(-OP_C(2).jsValue().toNumber(exec)));
}

SLOW_PATH_DECL(slow_path_add)
{
    BEGIN();
    JSValue v1 = OP_C(2).jsValue();
    JSValue v2 = OP_C(3).jsValue();
    
    if (v1.isString() && !v2.isObject())
        RETURN(jsString(exec, asString(v1), v2.toString(exec)));
    
    if (v1.isNumber() && v2.isNumber())
        RETURN(jsNumber(v1.asNumber() + v2.asNumber()));
    
    RETURN(jsAddSlowCase(exec, v1, v2));
}

// The following arithmetic and bitwise operations need to be sure to run
// toNumber() on their operands in order.  (A call to toNumber() is idempotent
// if an exception is already set on the ExecState.)

SLOW_PATH_DECL(slow_path_mul)
{
    BEGIN();
    double a = OP_C(2).jsValue().toNumber(exec);
    double b = OP_C(3).jsValue().toNumber(exec);
    RETURN(jsNumber(a * b));
}

SLOW_PATH_DECL(slow_path_sub)
{
    BEGIN();
    double a = OP_C(2).jsValue().toNumber(exec);
    double b = OP_C(3).jsValue().toNumber(exec);
    RETURN(jsNumber(a - b));
}

SLOW_PATH_DECL(slow_path_div)
{
    BEGIN();
    double a = OP_C(2).jsValue().toNumber(exec);
    double b = OP_C(3).jsValue().toNumber(exec);
    RETURN(jsNumber(a / b));
}

SLOW_PATH_DECL(slow_path_mod)
{
    BEGIN();
    double a = OP_C(2).jsValue().toNumber(exec);
    double b = OP_C(3).jsValue().toNumber(exec);
    RETURN(jsNumber(fmod(a, b)));
}

SLOW_PATH_DECL(slow_path_lshift)
{
    BEGIN();
    int32_t a = OP_C(2).jsValue().toInt32(exec);
    uint32_t b = OP_C(3).jsValue().toUInt32(exec);
    RETURN(jsNumber(a << (b & 31)));
}

SLOW_PATH_DECL(slow_path_rshift)
{
    BEGIN();
    int32_t a = OP_C(2).jsValue().toInt32(exec);
    uint32_t b = OP_C(3).jsValue().toUInt32(exec);
    RETURN(jsNumber(a >> (b & 31)));
}

SLOW_PATH_DECL(slow_path_urshift)
{
    BEGIN();
    uint32_t a = OP_C(2).jsValue().toUInt32(exec);
    uint32_t b = OP_C(3).jsValue().toUInt32(exec);
    RETURN(jsNumber(static_cast<int32_t>(a >> (b & 31))));
}

SLOW_PATH_DECL(slow_path_unsigned)
{
    BEGIN();
    uint32_t a = OP_C(2).jsValue().toUInt32(exec);
    RETURN(jsNumber(a));
}

SLOW_PATH_DECL(slow_path_bitand)
{
    BEGIN();
    int32_t a = OP_C(2).jsValue().toInt32(exec);
    int32_t b = OP_C(3).jsValue().toInt32(exec);
    RETURN(jsNumber(a & b));
}

SLOW_PATH_DECL(slow_path_bitor)
{
    BEGIN();
    int32_t a = OP_C(2).jsValue().toInt32(exec);
    int32_t b = OP_C(3).jsValue().toInt32(exec);
    RETURN(jsNumber(a | b));
}

SLOW_PATH_DECL(slow_path_bitxor)
{
    BEGIN();
    int32_t a = OP_C(2).jsValue().toInt32(exec);
    int32_t b = OP_C(3).jsValue().toInt32(exec);
    RETURN(jsNumber(a ^ b));
}

SLOW_PATH_DECL(slow_path_typeof)
{
    BEGIN();
    RETURN(jsTypeStringForValue(exec, OP_C(2).jsValue()));
}

SLOW_PATH_DECL(slow_path_is_object)
{
    BEGIN();
    RETURN(jsBoolean(jsIsObjectType(exec, OP_C(2).jsValue())));
}

SLOW_PATH_DECL(slow_path_is_function)
{
    BEGIN();
    RETURN(jsBoolean(jsIsFunctionType(OP_C(2).jsValue())));
}

SLOW_PATH_DECL(slow_path_in)
{
    BEGIN();
    RETURN(jsBoolean(CommonSlowPaths::opIn(exec, OP_C(2).jsValue(), OP_C(3).jsValue())));
}

SLOW_PATH_DECL(slow_path_del_by_val)
{
    BEGIN();
    JSValue baseValue = OP_C(2).jsValue();
    JSObject* baseObject = baseValue.toObject(exec);
    
    JSValue subscript = OP_C(3).jsValue();
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable()->deletePropertyByIndex(baseObject, exec, i);
    else if (isName(subscript))
        couldDelete = baseObject->methodTable()->deleteProperty(baseObject, exec, jsCast<NameInstance*>(subscript.asCell())->privateName());
    else {
        CHECK_EXCEPTION();
        Identifier property(exec, subscript.toString(exec)->value(exec));
        CHECK_EXCEPTION();
        couldDelete = baseObject->methodTable()->deleteProperty(baseObject, exec, property);
    }
    
    if (!couldDelete && exec->codeBlock()->isStrictMode())
        THROW(createTypeError(exec, "Unable to delete property."));
    
    RETURN(jsBoolean(couldDelete));
}

SLOW_PATH_DECL(slow_path_strcat)
{
    BEGIN();
    RETURN(jsStringFromRegisterArray(exec, &OP(2), pc[3].u.operand));
}

SLOW_PATH_DECL(slow_path_to_primitive)
{
    BEGIN();
    RETURN(OP_C(2).jsValue().toPrimitive(exec));
}

SLOW_PATH_DECL(slow_path_enter)
{
    BEGIN();
    ScriptExecutable* ownerExecutable = exec->codeBlock()->ownerExecutable();
    Heap::heap(ownerExecutable)->writeBarrier(ownerExecutable);
    END();
}

} // namespace JSC
