/*
 * Copyright (C) 2011-2016 Apple Inc. All rights reserved.
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
#include "ArrayConstructor.h"
#include "CallFrame.h"
#include "ClonedArguments.h"
#include "CodeProfiling.h"
#include "CommonSlowPathsExceptions.h"
#include "DirectArguments.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "ExceptionFuzz.h"
#include "GeneratorFrame.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "Interpreter.h"
#include "JIT.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSGlobalObjectFunctions.h"
#include "JSLexicalEnvironment.h"
#include "JSPropertyNameEnumerator.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCommon.h"
#include "LLIntExceptions.h"
#include "LowLevelInterpreter.h"
#include "ObjectConstructor.h"
#include "ScopedArguments.h"
#include "StructureRareDataInlines.h"
#include "TypeProfilerLog.h"
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
        doExceptionFuzzingIfEnabled(exec, "CommonSlowPaths", pc);   \
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

#define RETURN_WITH_PROFILING(value__, profilingAction__) do { \
        JSValue returnValue__ = (value__);  \
        CHECK_EXCEPTION();                  \
        OP(1) = returnValue__;              \
        profilingAction__;                  \
        END_IMPL();                         \
    } while (false)

#define RETURN(value) \
    RETURN_WITH_PROFILING(value, { })

#define RETURN_PROFILED(opcode__, value__) \
    RETURN_WITH_PROFILING(value__, PROFILE_VALUE(opcode__, returnValue__))

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
    if (vm.canUseJIT())
        result->thunkToCall = vm.getCTIStub(arityFixupGenerator).code().executableAddress();
    else
#endif
        result->thunkToCall = 0;
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

SLOW_PATH_DECL(slow_path_create_direct_arguments)
{
    BEGIN();
    RETURN(DirectArguments::createByCopying(exec));
}

SLOW_PATH_DECL(slow_path_create_scoped_arguments)
{
    BEGIN();
    JSLexicalEnvironment* scope = jsCast<JSLexicalEnvironment*>(OP(2).jsValue());
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    RETURN(ScopedArguments::createByCopying(exec, table, scope));
}

SLOW_PATH_DECL(slow_path_create_out_of_band_arguments)
{
    BEGIN();
    RETURN(ClonedArguments::createWithMachineFrame(exec, exec, ArgumentsMode::Cloned));
}

SLOW_PATH_DECL(slow_path_create_this)
{
    BEGIN();
    JSObject* result;
    JSCell* constructorAsCell = OP(2).jsValue().asCell();
    if (constructorAsCell->type() == JSFunctionType) {
        JSFunction* constructor = jsCast<JSFunction*>(constructorAsCell);
        auto& cacheWriteBarrier = pc[4].u.jsCell;
        if (!cacheWriteBarrier)
            cacheWriteBarrier.set(exec->vm(), exec->codeBlock(), constructor);
        else if (cacheWriteBarrier.unvalidatedGet() != JSCell::seenMultipleCalleeObjects() && cacheWriteBarrier.get() != constructor)
            cacheWriteBarrier.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());

        size_t inlineCapacity = pc[3].u.operand;
        Structure* structure = constructor->rareData(exec, inlineCapacity)->objectAllocationProfile()->structure();
        result = constructEmptyObject(exec, structure);
    } else
        result = constructEmptyObject(exec);
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_to_this)
{
    BEGIN();
    JSValue v1 = OP(1).jsValue();
    if (v1.isCell()) {
        Structure* myStructure = v1.asCell()->structure(vm);
        Structure* otherStructure = pc[2].u.structure.get();
        if (myStructure != otherStructure) {
            if (otherStructure)
                pc[3].u.toThisStatus = ToThisConflicted;
            pc[2].u.structure.set(vm, exec->codeBlock(), myStructure);
        }
    } else {
        pc[3].u.toThisStatus = ToThisConflicted;
        pc[2].u.structure.clear();
    }
    RETURN(v1.toThis(exec, exec->codeBlock()->isStrictMode() ? StrictMode : NotStrictMode));
}

SLOW_PATH_DECL(slow_path_throw_tdz_error)
{
    BEGIN();
    THROW(createTDZError(exec));
}

SLOW_PATH_DECL(slow_path_throw_strict_mode_readonly_property_write_error)
{
    BEGIN();
    THROW(createTypeError(exec, ASCIILiteral(StrictModeReadonlyPropertyWriteError)));
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

SLOW_PATH_DECL(slow_path_to_string)
{
    BEGIN();
    RETURN(OP_C(2).jsValue().toString(exec));
}

SLOW_PATH_DECL(slow_path_negate)
{
    BEGIN();
    RETURN(jsNumber(-OP_C(2).jsValue().toNumber(exec)));
}

#if ENABLE(DFG_JIT)
static void updateResultProfileForBinaryArithOp(ExecState* exec, Instruction* pc, JSValue result, JSValue left, JSValue right)
{
    CodeBlock* codeBlock = exec->codeBlock();
    unsigned bytecodeOffset = codeBlock->bytecodeOffset(pc);
    ResultProfile* profile = codeBlock->ensureResultProfile(bytecodeOffset);

    if (result.isNumber()) {
        if (!result.isInt32()) {
            if (left.isInt32() && right.isInt32())
                profile->setObservedInt32Overflow();

            double doubleVal = result.asNumber();
            if (!doubleVal && std::signbit(doubleVal))
                profile->setObservedNegZeroDouble();
            else {
                profile->setObservedNonNegZeroDouble();

                // The Int52 overflow check here intentionally omits 1ll << 51 as a valid negative Int52 value.
                // Therefore, we will get a false positive if the result is that value. This is intentionally
                // done to simplify the checking algorithm.
                static const int64_t int52OverflowPoint = (1ll << 51);
                int64_t int64Val = static_cast<int64_t>(std::abs(doubleVal));
                if (int64Val >= int52OverflowPoint)
                    profile->setObservedInt52Overflow();
            }
        }
    } else
        profile->setObservedNonNumber();
}
#else
static void updateResultProfileForBinaryArithOp(ExecState*, Instruction*, JSValue, JSValue, JSValue) { }
#endif

SLOW_PATH_DECL(slow_path_add)
{
    BEGIN();
    JSValue v1 = OP_C(2).jsValue();
    JSValue v2 = OP_C(3).jsValue();
    JSValue result;

    if (v1.isString() && !v2.isObject())
        result = jsString(exec, asString(v1), v2.toString(exec));
    else if (v1.isNumber() && v2.isNumber())
        result = jsNumber(v1.asNumber() + v2.asNumber());
    else
        result = jsAddSlowCase(exec, v1, v2);

    RETURN_WITH_PROFILING(result, {
        updateResultProfileForBinaryArithOp(exec, pc, result, v1, v2);
    });
}

// The following arithmetic and bitwise operations need to be sure to run
// toNumber() on their operands in order.  (A call to toNumber() is idempotent
// if an exception is already set on the ExecState.)

SLOW_PATH_DECL(slow_path_mul)
{
    BEGIN();
    JSValue left = OP_C(2).jsValue();
    JSValue right = OP_C(3).jsValue();
    double a = left.toNumber(exec);
    double b = right.toNumber(exec);
    JSValue result = jsNumber(a * b);
    RETURN_WITH_PROFILING(result, {
        updateResultProfileForBinaryArithOp(exec, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_sub)
{
    BEGIN();
    JSValue left = OP_C(2).jsValue();
    JSValue right = OP_C(3).jsValue();
    double a = left.toNumber(exec);
    double b = right.toNumber(exec);
    JSValue result = jsNumber(a - b);
    RETURN_WITH_PROFILING(result, {
        updateResultProfileForBinaryArithOp(exec, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_div)
{
    BEGIN();
    JSValue left = OP_C(2).jsValue();
    JSValue right = OP_C(3).jsValue();
    double a = left.toNumber(exec);
    double b = right.toNumber(exec);
    JSValue result = jsNumber(a / b);
    RETURN_WITH_PROFILING(result, {
        updateResultProfileForBinaryArithOp(exec, pc, result, left, right);
    });
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

SLOW_PATH_DECL(slow_path_is_object_or_null)
{
    BEGIN();
    RETURN(jsBoolean(jsIsObjectTypeOrNull(exec, OP_C(2).jsValue())));
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
    else {
        CHECK_EXCEPTION();
        auto property = subscript.toPropertyKey(exec);
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
    CodeBlock* codeBlock = exec->codeBlock();
    Heap::heap(codeBlock)->writeBarrier(codeBlock);
    END();
}

SLOW_PATH_DECL(slow_path_get_enumerable_length)
{
    BEGIN();
    JSValue enumeratorValue = OP(2).jsValue();
    if (enumeratorValue.isUndefinedOrNull())
        RETURN(jsNumber(0));

    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(enumeratorValue.asCell());

    RETURN(jsNumber(enumerator->indexedLength()));
}

SLOW_PATH_DECL(slow_path_has_indexed_property)
{
    BEGIN();
    JSObject* base = OP(2).jsValue().toObject(exec);
    JSValue property = OP(3).jsValue();
    pc[4].u.arrayProfile->observeStructure(base->structure(vm));
    ASSERT(property.isUInt32());
    RETURN(jsBoolean(base->hasPropertyGeneric(exec, property.asUInt32(), PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_structure_property)
{
    BEGIN();
    JSObject* base = OP(2).jsValue().toObject(exec);
    JSValue property = OP(3).jsValue();
    ASSERT(property.isString());
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(OP(4).jsValue().asCell());
    if (base->structure(vm)->id() == enumerator->cachedStructureID())
        RETURN(jsBoolean(true));
    RETURN(jsBoolean(base->hasPropertyGeneric(exec, asString(property.asCell())->toIdentifier(exec), PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_generic_property)
{
    BEGIN();
    JSObject* base = OP(2).jsValue().toObject(exec);
    JSValue property = OP(3).jsValue();
    bool result;
    if (property.isString())
        result = base->hasPropertyGeneric(exec, asString(property.asCell())->toIdentifier(exec), PropertySlot::InternalMethodType::GetOwnProperty);
    else {
        ASSERT(property.isUInt32());
        result = base->hasPropertyGeneric(exec, property.asUInt32(), PropertySlot::InternalMethodType::GetOwnProperty);
    }
    RETURN(jsBoolean(result));
}

SLOW_PATH_DECL(slow_path_get_direct_pname)
{
    BEGIN();
    JSValue baseValue = OP_C(2).jsValue();
    JSValue property = OP(3).jsValue();
    ASSERT(property.isString());
    RETURN(baseValue.get(exec, asString(property)->toIdentifier(exec)));
}

SLOW_PATH_DECL(slow_path_get_property_enumerator)
{
    BEGIN();
    JSValue baseValue = OP(2).jsValue();
    if (baseValue.isUndefinedOrNull())
        RETURN(JSPropertyNameEnumerator::create(vm));

    JSObject* base = baseValue.toObject(exec);

    RETURN(propertyNameEnumerator(exec, base));
}

SLOW_PATH_DECL(slow_path_next_structure_enumerator_pname)
{
    BEGIN();
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(OP(2).jsValue().asCell());
    uint32_t index = OP(3).jsValue().asUInt32();

    JSString* propertyName = nullptr;
    if (index < enumerator->endStructurePropertyIndex())
        propertyName = enumerator->propertyNameAtIndex(index);
    RETURN(propertyName ? propertyName : jsNull());
}

SLOW_PATH_DECL(slow_path_next_generic_enumerator_pname)
{
    BEGIN();
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(OP(2).jsValue().asCell());
    uint32_t index = OP(3).jsValue().asUInt32();

    JSString* propertyName = nullptr;
    if (enumerator->endStructurePropertyIndex() <= index && index < enumerator->endGenericPropertyIndex())
        propertyName = enumerator->propertyNameAtIndex(index);
    RETURN(propertyName ? propertyName : jsNull());
}

SLOW_PATH_DECL(slow_path_to_index_string)
{
    BEGIN();
    RETURN(jsString(exec, Identifier::from(exec, OP(2).jsValue().asUInt32()).string()));
}

SLOW_PATH_DECL(slow_path_profile_type_clear_log)
{
    BEGIN();
    vm.typeProfilerLog()->processLogEntries(ASCIILiteral("LLInt log full."));
    END();
}

SLOW_PATH_DECL(slow_path_assert)
{
    BEGIN();
    ASSERT_WITH_MESSAGE(OP(1).jsValue().asBoolean(), "JS assertion failed at line %d in:\n%s\n", pc[2].u.operand, exec->codeBlock()->sourceCodeForTools().data());
    END();
}

SLOW_PATH_DECL(slow_path_save)
{
    // Only save variables and temporary registers. The scope registers are included in them.
    // But parameters are not included. Because the generator implementation replaces the values of parameters on each generator.next() call.
    BEGIN();
    JSValue generator = OP(1).jsValue();
    GeneratorFrame* frame = nullptr;
    JSValue value = generator.get(exec, exec->propertyNames().generatorFramePrivateName);
    if (!value.isNull())
        frame = jsCast<GeneratorFrame*>(value);
    else {
        // FIXME: Once JSGenerator specialized object is introduced, this GeneratorFrame should be embeded into it to avoid allocations.
        // https://bugs.webkit.org/show_bug.cgi?id=151545
        frame = GeneratorFrame::create(exec->vm(),  exec->codeBlock()->numCalleeLocals());
        PutPropertySlot slot(generator, true, PutPropertySlot::PutById);
        asObject(generator)->methodTable(exec->vm())->put(asObject(generator), exec, exec->propertyNames().generatorFramePrivateName, frame, slot);
    }
    unsigned liveCalleeLocalsIndex = pc[2].u.unsignedValue;
    frame->save(exec, exec->codeBlock()->liveCalleeLocalsAtYield(liveCalleeLocalsIndex));
    END();
}

SLOW_PATH_DECL(slow_path_resume)
{
    BEGIN();
    JSValue generator = OP(1).jsValue();
    GeneratorFrame* frame = jsCast<GeneratorFrame*>(generator.get(exec, exec->propertyNames().generatorFramePrivateName));
    unsigned liveCalleeLocalsIndex = pc[2].u.unsignedValue;
    frame->resume(exec, exec->codeBlock()->liveCalleeLocalsAtYield(liveCalleeLocalsIndex));
    END();
}

SLOW_PATH_DECL(slow_path_create_lexical_environment)
{
    BEGIN();
    int scopeReg = pc[2].u.operand;
    JSScope* currentScope = exec->uncheckedR(scopeReg).Register::scope();
    SymbolTable* symbolTable = jsCast<SymbolTable*>(OP_C(3).jsValue());
    JSValue initialValue = OP_C(4).jsValue();
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    JSScope* newScope = JSLexicalEnvironment::create(vm, exec->lexicalGlobalObject(), currentScope, symbolTable, initialValue);
    RETURN(newScope);
}

SLOW_PATH_DECL(slow_path_push_with_scope)
{
    BEGIN();
    JSObject* newScope = OP_C(2).jsValue().toObject(exec);
    CHECK_EXCEPTION();

    int scopeReg = pc[3].u.operand;
    JSScope* currentScope = exec->uncheckedR(scopeReg).Register::scope();
    RETURN(JSWithScope::create(exec, newScope, currentScope));
}

SLOW_PATH_DECL(slow_path_resolve_scope)
{
    BEGIN();
    const Identifier& ident = exec->codeBlock()->identifier(pc[3].u.operand);
    JSScope* scope = exec->uncheckedR(pc[2].u.operand).Register::scope();
    JSValue resolvedScope = JSScope::resolve(exec, scope, ident);

    ResolveType resolveType = static_cast<ResolveType>(pc[4].u.operand);

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(resolveType != ModuleVar);

    if (resolveType == UnresolvedProperty || resolveType == UnresolvedPropertyWithVarInjectionChecks) {
        if (JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsDynamicCast<JSGlobalLexicalEnvironment*>(resolvedScope)) {
            if (resolveType == UnresolvedProperty)
                pc[4].u.operand = GlobalLexicalVar;
            else
                pc[4].u.operand = GlobalLexicalVarWithVarInjectionChecks;
            pc[6].u.pointer = globalLexicalEnvironment;
        } else if (JSGlobalObject* globalObject = jsDynamicCast<JSGlobalObject*>(resolvedScope)) {
            if (globalObject->hasProperty(exec, ident)) {
                if (resolveType == UnresolvedProperty)
                    pc[4].u.operand = GlobalProperty;
                else
                    pc[4].u.operand = GlobalPropertyWithVarInjectionChecks;

                pc[6].u.pointer = globalObject;
            }
        }
    }

    RETURN(resolvedScope);
}

SLOW_PATH_DECL(slow_path_copy_rest)
{
    BEGIN();
    unsigned arraySize = OP_C(2).jsValue().asUInt32();
    if (!arraySize) {
        ASSERT(!jsCast<JSArray*>(OP(1).jsValue())->length());
        END();
    }
    JSArray* array = jsCast<JSArray*>(OP(1).jsValue());
    ASSERT(arraySize == array->length());
    unsigned numParamsToSkip = pc[3].u.unsignedValue;
    for (unsigned i = 0; i < arraySize; i++)
        array->putDirectIndex(exec, i, exec->uncheckedArgument(i + numParamsToSkip));
    END();
}

} // namespace JSC
