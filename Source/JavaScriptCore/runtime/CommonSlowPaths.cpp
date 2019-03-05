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
#include "CommonSlowPaths.h"

#include "ArithProfile.h"
#include "ArrayConstructor.h"
#include "BuiltinNames.h"
#include "BytecodeStructs.h"
#include "CallFrame.h"
#include "ClonedArguments.h"
#include "CodeProfiling.h"
#include "DefinePropertyAttributes.h"
#include "DirectArguments.h"
#include "Error.h"
#include "ErrorHandlingScope.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "GetterSetter.h"
#include "HostCallReturnValue.h"
#include "ICStats.h"
#include "Interpreter.h"
#include "IteratorOperations.h"
#include "JIT.h"
#include "JSArrayInlines.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSFixedArray.h"
#include "JSGlobalObjectFunctions.h"
#include "JSImmutableButterfly.h"
#include "JSLexicalEnvironment.h"
#include "JSPropertyNameEnumerator.h"
#include "JSString.h"
#include "JSWithScope.h"
#include "LLIntCommon.h"
#include "LLIntExceptions.h"
#include "LowLevelInterpreter.h"
#include "MathCommon.h"
#include "ObjectConstructor.h"
#include "OpcodeInlines.h"
#include "ScopedArguments.h"
#include "StructureRareDataInlines.h"
#include "ThunkGenerators.h"
#include "TypeProfilerLog.h"
#include <wtf/StringPrintStream.h>
#include <wtf/Variant.h>

namespace JSC {

#define BEGIN_NO_SET_PC() \
    VM& vm = exec->vm();      \
    NativeCallFrameTracer tracer(&vm, exec); \
    auto throwScope = DECLARE_THROW_SCOPE(vm); \
    UNUSED_PARAM(throwScope)

#ifndef NDEBUG
#define SET_PC_FOR_STUBS() do { \
        exec->codeBlock()->bytecodeOffset(pc); \
        exec->setCurrentVPC(pc); \
    } while (false)
#else
#define SET_PC_FOR_STUBS() do { \
        exec->setCurrentVPC(pc); \
    } while (false)
#endif

#define RETURN_TO_THROW(exec, pc)   pc = LLInt::returnToThrow(exec)

#define BEGIN()                           \
    BEGIN_NO_SET_PC();                    \
    SET_PC_FOR_STUBS()

#define GET(operand) (exec->uncheckedR(operand.offset()))
#define GET_C(operand) (exec->r(operand.offset()))

#define RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define END_IMPL() RETURN_TWO(pc, exec)

#define THROW(exceptionToThrow) do {                        \
        throwException(exec, throwScope, exceptionToThrow); \
        RETURN_TO_THROW(exec, pc);                          \
        END_IMPL();                                         \
    } while (false)

#define CHECK_EXCEPTION() do {                    \
        doExceptionFuzzingIfEnabled(exec, throwScope, "CommonSlowPaths", pc);   \
        if (UNLIKELY(throwScope.exception())) {   \
            RETURN_TO_THROW(exec, pc);            \
            END_IMPL();                           \
        }                                         \
    } while (false)

#define END() do {                        \
        CHECK_EXCEPTION();                \
        END_IMPL();                       \
    } while (false)

#define BRANCH(condition) do {                      \
        bool bCondition = (condition);                         \
        CHECK_EXCEPTION();                                  \
        if (bCondition)                                        \
            pc = bytecode.m_targetLabel \
                ? reinterpret_cast<const Instruction*>(reinterpret_cast<const uint8_t*>(pc) + bytecode.m_targetLabel) \
                : exec->codeBlock()->outOfLineJumpTarget(pc);                              \
        else                                                      \
            pc = reinterpret_cast<const Instruction*>(reinterpret_cast<const uint8_t*>(pc) + pc->size()); \
        END_IMPL();                                         \
    } while (false)

#define RETURN_WITH_PROFILING_CUSTOM(result__, value__, profilingAction__) do { \
        JSValue returnValue__ = (value__);  \
        CHECK_EXCEPTION();                  \
        GET(result__) = returnValue__;              \
        profilingAction__;                  \
        END_IMPL();                         \
    } while (false)

#define RETURN_WITH_PROFILING(value__, profilingAction__) RETURN_WITH_PROFILING_CUSTOM(bytecode.m_dst, value__, profilingAction__)

#define RETURN(value) \
    RETURN_WITH_PROFILING(value, { })

#define RETURN_PROFILED(value__) \
    RETURN_WITH_PROFILING(value__, PROFILE_VALUE(returnValue__))

#define PROFILE_VALUE(value) do { \
        bytecode.metadata(exec).m_profile.m_buckets[0] = JSValue::encode(value); \
    } while (false)

#define CALL_END_IMPL(exec, callTarget, callTargetTag) \
    RETURN_TWO(retagCodePtr((callTarget), callTargetTag, SlowPathPtrTag), (exec))

#define CALL_CHECK_EXCEPTION(exec, pc) do {                          \
        ExecState* cceExec = (exec);                                 \
        Instruction* ccePC = (pc);                                   \
        if (UNLIKELY(throwScope.exception()))                        \
            CALL_END_IMPL(cceExec, LLInt::callToThrow(cceExec), ExceptionHandlerPtrTag); \
    } while (false)

static void throwArityCheckStackOverflowError(ExecState* exec, ThrowScope& scope)
{
    JSObject* error = createStackOverflowError(exec);
    throwException(exec, scope, error);
#if LLINT_TRACING
    if (UNLIKELY(Options::traceLLIntSlowPath()))
        dataLog("Throwing exception ", JSValue(scope.exception()), ".\n");
#endif
}

SLOW_PATH_DECL(slow_path_call_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(exec, vm, CodeForCall);
    if (UNLIKELY(slotsToAdd < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(exec, CodeForCall);
        exec->convertToStackOverflowFrame(vm, codeBlock);
        NativeCallFrameTracer tracer(&vm, exec);
        ErrorHandlingScope errorScope(vm);
        throwScope.release();
        throwArityCheckStackOverflowError(exec, throwScope);
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    RETURN_TWO(0, bitwise_cast<void*>(static_cast<uintptr_t>(slotsToAdd)));
}

SLOW_PATH_DECL(slow_path_construct_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(exec, vm, CodeForConstruct);
    if (UNLIKELY(slotsToAdd < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(exec, CodeForConstruct);
        exec->convertToStackOverflowFrame(vm, codeBlock);
        NativeCallFrameTracer tracer(&vm, exec);
        ErrorHandlingScope errorScope(vm);
        throwArityCheckStackOverflowError(exec, throwScope);
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), exec);
    }
    RETURN_TWO(0, bitwise_cast<void*>(static_cast<uintptr_t>(slotsToAdd)));
}

SLOW_PATH_DECL(slow_path_create_direct_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateDirectArguments>();
    RETURN(DirectArguments::createByCopying(exec));
}

SLOW_PATH_DECL(slow_path_create_scoped_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateScopedArguments>();
    JSLexicalEnvironment* scope = jsCast<JSLexicalEnvironment*>(GET(bytecode.m_scope).jsValue());
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    RETURN(ScopedArguments::createByCopying(exec, table, scope));
}

SLOW_PATH_DECL(slow_path_create_cloned_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateClonedArguments>();
    RETURN(ClonedArguments::createWithMachineFrame(exec, exec, ArgumentsMode::Cloned));
}

SLOW_PATH_DECL(slow_path_create_this)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateThis>();
    JSObject* result;
    JSObject* constructorAsObject = asObject(GET(bytecode.m_callee).jsValue());
    if (constructorAsObject->type() == JSFunctionType && jsCast<JSFunction*>(constructorAsObject)->canUseAllocationProfile()) {
        JSFunction* constructor = jsCast<JSFunction*>(constructorAsObject);
        WriteBarrier<JSCell>& cachedCallee = bytecode.metadata(exec).m_cachedCallee;
        if (!cachedCallee)
            cachedCallee.set(vm, exec->codeBlock(), constructor);
        else if (cachedCallee.unvalidatedGet() != JSCell::seenMultipleCalleeObjects() && cachedCallee.get() != constructor)
            cachedCallee.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());

        size_t inlineCapacity = bytecode.m_inlineCapacity;
        ObjectAllocationProfile* allocationProfile = constructor->ensureRareDataAndAllocationProfile(exec, inlineCapacity)->objectAllocationProfile();
        throwScope.releaseAssertNoException();
        Structure* structure = allocationProfile->structure();
        result = constructEmptyObject(exec, structure);
        if (structure->hasPolyProto()) {
            JSObject* prototype = allocationProfile->prototype();
            ASSERT(prototype == constructor->prototypeForConstruction(vm, exec));
            result->putDirect(vm, knownPolyProtoOffset, prototype);
            prototype->didBecomePrototype();
            ASSERT_WITH_MESSAGE(!hasIndexedProperties(result->indexingType()), "We rely on JSFinalObject not starting out with an indexing type otherwise we would potentially need to convert to slow put storage");
        }
    } else {
        // http://ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
        JSValue proto = constructorAsObject->get(exec, vm.propertyNames->prototype);
        CHECK_EXCEPTION();
        if (proto.isObject())
            result = constructEmptyObject(exec, asObject(proto));
        else
            result = constructEmptyObject(exec);
    }
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_to_this)
{
    BEGIN();
    auto bytecode = pc->as<OpToThis>();
    auto& metadata = bytecode.metadata(exec);
    JSValue v1 = GET(bytecode.m_srcDst).jsValue();
    if (v1.isCell()) {
        Structure* myStructure = v1.asCell()->structure(vm);
        Structure* otherStructure = metadata.m_cachedStructure.get();
        if (myStructure != otherStructure) {
            if (otherStructure)
                metadata.m_toThisStatus = ToThisConflicted;
            metadata.m_cachedStructure.set(vm, exec->codeBlock(), myStructure);
        }
    } else {
        metadata.m_toThisStatus = ToThisConflicted;
        metadata.m_cachedStructure.clear();
    }
    // Note: We only need to do this value profiling here on the slow path. The fast path
    // just returns the input to to_this if the structure check succeeds. If the structure
    // check succeeds, doing value profiling here is equivalent to doing it with a potentially
    // different object that still has the same structure on the fast path since it'll produce
    // the same SpeculatedType. Therefore, we don't need to worry about value profiling on the
    // fast path.
    auto value = v1.toThis(exec, exec->codeBlock()->isStrictMode() ? StrictMode : NotStrictMode);
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, value, PROFILE_VALUE(value));
}

SLOW_PATH_DECL(slow_path_throw_tdz_error)
{
    BEGIN();
    THROW(createTDZError(exec));
}

SLOW_PATH_DECL(slow_path_check_tdz)
{
    BEGIN();
    THROW(createTDZError(exec));
}

SLOW_PATH_DECL(slow_path_throw_strict_mode_readonly_property_write_error)
{
    BEGIN();
    THROW(createTypeError(exec, ReadonlyPropertyWriteError));
}

SLOW_PATH_DECL(slow_path_not)
{
    BEGIN();
    auto bytecode = pc->as<OpNot>();
    RETURN(jsBoolean(!GET_C(bytecode.m_operand).jsValue().toBoolean(exec)));
}

SLOW_PATH_DECL(slow_path_eq)
{
    BEGIN();
    auto bytecode = pc->as<OpEq>();
    RETURN(jsBoolean(JSValue::equal(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_neq)
{
    BEGIN();
    auto bytecode = pc->as<OpNeq>();
    RETURN(jsBoolean(!JSValue::equal(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_stricteq)
{
    BEGIN();
    auto bytecode = pc->as<OpStricteq>();
    RETURN(jsBoolean(JSValue::strictEqual(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_nstricteq)
{
    BEGIN();
    auto bytecode = pc->as<OpNstricteq>();
    RETURN(jsBoolean(!JSValue::strictEqual(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_less)
{
    BEGIN();
    auto bytecode = pc->as<OpLess>();
    RETURN(jsBoolean(jsLess<true>(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_lesseq)
{
    BEGIN();
    auto bytecode = pc->as<OpLesseq>();
    RETURN(jsBoolean(jsLessEq<true>(exec, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_greater)
{
    BEGIN();
    auto bytecode = pc->as<OpGreater>();
    RETURN(jsBoolean(jsLess<false>(exec, GET_C(bytecode.m_rhs).jsValue(), GET_C(bytecode.m_lhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_greatereq)
{
    BEGIN();
    auto bytecode = pc->as<OpGreatereq>();
    RETURN(jsBoolean(jsLessEq<false>(exec, GET_C(bytecode.m_rhs).jsValue(), GET_C(bytecode.m_lhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_inc)
{
    BEGIN();
    auto bytecode = pc->as<OpInc>();
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, jsNumber(GET(bytecode.m_srcDst).jsValue().toNumber(exec) + 1), { });
}

SLOW_PATH_DECL(slow_path_dec)
{
    BEGIN();
    auto bytecode = pc->as<OpDec>();
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, jsNumber(GET(bytecode.m_srcDst).jsValue().toNumber(exec) - 1), { });
}

SLOW_PATH_DECL(slow_path_to_string)
{
    BEGIN();
    auto bytecode = pc->as<OpToString>();
    RETURN(GET_C(bytecode.m_operand).jsValue().toString(exec));
}

#if ENABLE(JIT)
static void updateArithProfileForUnaryArithOp(OpNegate::Metadata& metadata, JSValue result, JSValue operand)
{
    ArithProfile& profile = metadata.m_arithProfile;
    profile.observeLHS(operand);
    ASSERT(result.isNumber() || result.isBigInt());
    if (result.isNumber()) {
        if (!result.isInt32()) {
            if (operand.isInt32())
                profile.setObservedInt32Overflow();
            
            double doubleVal = result.asNumber();
            if (!doubleVal && std::signbit(doubleVal))
                profile.setObservedNegZeroDouble();
            else {
                profile.setObservedNonNegZeroDouble();
                
                // The Int52 overflow check here intentionally omits 1ll << 51 as a valid negative Int52 value.
                // Therefore, we will get a false positive if the result is that value. This is intentionally
                // done to simplify the checking algorithm.
                static const int64_t int52OverflowPoint = (1ll << 51);
                int64_t int64Val = static_cast<int64_t>(std::abs(doubleVal));
                if (int64Val >= int52OverflowPoint)
                    profile.setObservedInt52Overflow();
            }
        }
    } else if (result.isBigInt())
        profile.setObservedBigInt();
    else
        profile.setObservedNonNumeric();
}
#else
static void updateArithProfileForUnaryArithOp(OpNegate::Metadata&, JSValue, JSValue) { }
#endif

SLOW_PATH_DECL(slow_path_negate)
{
    BEGIN();
    auto bytecode = pc->as<OpNegate>();
    auto& metadata = bytecode.metadata(exec);
    JSValue operand = GET_C(bytecode.m_operand).jsValue();
    JSValue primValue = operand.toPrimitive(exec, PreferNumber);
    CHECK_EXCEPTION();

    if (primValue.isBigInt()) {
        JSBigInt* result = JSBigInt::unaryMinus(vm, asBigInt(primValue));
        RETURN_WITH_PROFILING(result, {
            updateArithProfileForUnaryArithOp(metadata, result, operand);
        });
    }
    
    JSValue result = jsNumber(-primValue.toNumber(exec));
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForUnaryArithOp(metadata, result, operand);
    });
}

#if ENABLE(DFG_JIT)
static void updateArithProfileForBinaryArithOp(ExecState* exec, const Instruction* pc, JSValue result, JSValue left, JSValue right)
{
    CodeBlock* codeBlock = exec->codeBlock();
    ArithProfile& profile = *codeBlock->arithProfileForPC(pc);

    if (result.isNumber()) {
        if (!result.isInt32()) {
            if (left.isInt32() && right.isInt32())
                profile.setObservedInt32Overflow();

            double doubleVal = result.asNumber();
            if (!doubleVal && std::signbit(doubleVal))
                profile.setObservedNegZeroDouble();
            else {
                profile.setObservedNonNegZeroDouble();

                // The Int52 overflow check here intentionally omits 1ll << 51 as a valid negative Int52 value.
                // Therefore, we will get a false positive if the result is that value. This is intentionally
                // done to simplify the checking algorithm.
                static const int64_t int52OverflowPoint = (1ll << 51);
                int64_t int64Val = static_cast<int64_t>(std::abs(doubleVal));
                if (int64Val >= int52OverflowPoint)
                    profile.setObservedInt52Overflow();
            }
        }
    } else if (result.isBigInt())
        profile.setObservedBigInt();
    else 
        profile.setObservedNonNumeric();
}
#else
static void updateArithProfileForBinaryArithOp(ExecState*, const Instruction*, JSValue, JSValue, JSValue) { }
#endif

SLOW_PATH_DECL(slow_path_to_number)
{
    BEGIN();
    auto bytecode = pc->as<OpToNumber>();
    JSValue argument = GET_C(bytecode.m_operand).jsValue();
    JSValue result = jsNumber(argument.toNumber(exec));
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_to_object)
{
    BEGIN();
    auto bytecode = pc->as<OpToObject>();
    JSValue argument = GET_C(bytecode.m_operand).jsValue();
    if (UNLIKELY(argument.isUndefinedOrNull())) {
        const Identifier& ident = exec->codeBlock()->identifier(bytecode.m_message);
        if (!ident.isEmpty())
            THROW(createTypeError(exec, ident.impl()));
    }
    JSObject* result = argument.toObject(exec);
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_add)
{
    BEGIN();
    auto bytecode = pc->as<OpAdd>();
    JSValue v1 = GET_C(bytecode.m_lhs).jsValue();
    JSValue v2 = GET_C(bytecode.m_rhs).jsValue();

    ArithProfile& arithProfile = *exec->codeBlock()->arithProfileForPC(pc);
    arithProfile.observeLHSAndRHS(v1, v2);

    JSValue result = jsAdd(exec, v1, v2);

    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(exec, pc, result, v1, v2);
    });
}

// The following arithmetic and bitwise operations need to be sure to run
// toNumber() on their operands in order.  (A call to toNumber() is idempotent
// if an exception is already set on the ExecState.)

SLOW_PATH_DECL(slow_path_mul)
{
    BEGIN();
    auto bytecode = pc->as<OpMul>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsMul(exec, left, right);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(exec, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_sub)
{
    BEGIN();
    auto bytecode = pc->as<OpSub>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    auto leftNumeric = left.toNumeric(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = right.toNumeric(exec);
    CHECK_EXCEPTION();

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::sub(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN_WITH_PROFILING(result, {
                updateArithProfileForBinaryArithOp(exec, pc, result, left, right);
            });
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in subtraction."));
    }

    JSValue result = jsNumber(WTF::get<double>(leftNumeric) - WTF::get<double>(rightNumeric));
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(exec, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_div)
{
    BEGIN();
    auto bytecode = pc->as<OpDiv>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    auto leftNumeric = left.toNumeric(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = right.toNumeric(exec);
    CHECK_EXCEPTION();

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::divide(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN_WITH_PROFILING(result, {
                updateArithProfileForBinaryArithOp(exec, pc, result, left, right);
            });
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in division."));
    }

    double a = WTF::get<double>(leftNumeric);
    double b = WTF::get<double>(rightNumeric);
    JSValue result = jsNumber(a / b);
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(exec, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_mod)
{
    BEGIN();
    auto bytecode = pc->as<OpMod>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    auto leftNumeric = left.toNumeric(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = right.toNumeric(exec);
    CHECK_EXCEPTION();
    
    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::remainder(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in remainder operation."));
    }
    
    double a = WTF::get<double>(leftNumeric);
    double b = WTF::get<double>(rightNumeric);
    RETURN(jsNumber(jsMod(a, b)));
}

SLOW_PATH_DECL(slow_path_pow)
{
    BEGIN();
    auto bytecode = pc->as<OpPow>();
    double a = GET_C(bytecode.m_lhs).jsValue().toNumber(exec);
    if (UNLIKELY(throwScope.exception()))
        RETURN(JSValue());
    double b = GET_C(bytecode.m_rhs).jsValue().toNumber(exec);
    if (UNLIKELY(throwScope.exception()))
        RETURN(JSValue());
    RETURN(jsNumber(operationMathPow(a, b)));
}

SLOW_PATH_DECL(slow_path_lshift)
{
    BEGIN();
    auto bytecode = pc->as<OpLshift>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    auto leftNumeric = left.toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = right.toBigIntOrInt32(exec);
    CHECK_EXCEPTION();

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::leftShift(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in left shift operation."));
    }

    RETURN(jsNumber(WTF::get<int32_t>(leftNumeric) << (WTF::get<int32_t>(rightNumeric) & 31)));
}

SLOW_PATH_DECL(slow_path_rshift)
{
    BEGIN();
    auto bytecode = pc->as<OpRshift>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    auto leftNumeric = left.toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = right.toBigIntOrInt32(exec);
    CHECK_EXCEPTION();

    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::signedRightShift(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in signed right shift operation."));
    }

    RETURN(jsNumber(WTF::get<int32_t>(leftNumeric) >> (WTF::get<int32_t>(rightNumeric) & 31)));
}

SLOW_PATH_DECL(slow_path_urshift)
{
    BEGIN();
    auto bytecode = pc->as<OpUrshift>();
    uint32_t a = GET_C(bytecode.m_lhs).jsValue().toUInt32(exec);
    if (UNLIKELY(throwScope.exception()))
        RETURN(JSValue());
    uint32_t b = GET_C(bytecode.m_rhs).jsValue().toUInt32(exec);
    RETURN(jsNumber(static_cast<int32_t>(a >> (b & 31))));
}

SLOW_PATH_DECL(slow_path_unsigned)
{
    BEGIN();
    auto bytecode = pc->as<OpUnsigned>();
    uint32_t a = GET_C(bytecode.m_operand).jsValue().toUInt32(exec);
    RETURN(jsNumber(a));
}

SLOW_PATH_DECL(slow_path_bitnot)
{
    BEGIN();
    auto bytecode = pc->as<OpBitnot>();
    int32_t operand = GET_C(bytecode.m_operand).jsValue().toInt32(exec);
    CHECK_EXCEPTION();
    RETURN_PROFILED(jsNumber(~operand));
}

SLOW_PATH_DECL(slow_path_bitand)
{
    BEGIN();
    auto bytecode = pc->as<OpBitand>();
    auto leftNumeric = GET_C(bytecode.m_lhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = GET_C(bytecode.m_rhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::bitwiseAnd(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN_PROFILED(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in bitwise 'and' operation."));
    }

    RETURN_PROFILED(jsNumber(WTF::get<int32_t>(leftNumeric) & WTF::get<int32_t>(rightNumeric)));
}

SLOW_PATH_DECL(slow_path_bitor)
{
    BEGIN();
    auto bytecode = pc->as<OpBitor>();
    auto leftNumeric = GET_C(bytecode.m_lhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = GET_C(bytecode.m_rhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::bitwiseOr(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN_PROFILED(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in bitwise 'or' operation."));
    }

    RETURN_PROFILED(jsNumber(WTF::get<int32_t>(leftNumeric) | WTF::get<int32_t>(rightNumeric)));
}

SLOW_PATH_DECL(slow_path_bitxor)
{
    BEGIN();
    auto bytecode = pc->as<OpBitxor>();
    auto leftNumeric = GET_C(bytecode.m_lhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    auto rightNumeric = GET_C(bytecode.m_rhs).jsValue().toBigIntOrInt32(exec);
    CHECK_EXCEPTION();
    if (WTF::holds_alternative<JSBigInt*>(leftNumeric) || WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
        if (WTF::holds_alternative<JSBigInt*>(leftNumeric) && WTF::holds_alternative<JSBigInt*>(rightNumeric)) {
            JSBigInt* result = JSBigInt::bitwiseXor(exec, WTF::get<JSBigInt*>(leftNumeric), WTF::get<JSBigInt*>(rightNumeric));
            CHECK_EXCEPTION();
            RETURN_PROFILED(result);
        }

        THROW(createTypeError(exec, "Invalid mix of BigInt and other type in bitwise 'xor' operation."));
    }

    RETURN_PROFILED(jsNumber(WTF::get<int32_t>(leftNumeric) ^ WTF::get<int32_t>(rightNumeric)));
}

SLOW_PATH_DECL(slow_path_typeof)
{
    BEGIN();
    auto bytecode = pc->as<OpTypeof>();
    RETURN(jsTypeStringForValue(exec, GET_C(bytecode.m_value).jsValue()));
}

SLOW_PATH_DECL(slow_path_is_object_or_null)
{
    BEGIN();
    auto bytecode = pc->as<OpIsObjectOrNull>();
    RETURN(jsBoolean(jsIsObjectTypeOrNull(exec, GET_C(bytecode.m_operand).jsValue())));
}

SLOW_PATH_DECL(slow_path_is_function)
{
    BEGIN();
    auto bytecode = pc->as<OpIsFunction>();
    RETURN(jsBoolean(GET_C(bytecode.m_operand).jsValue().isFunction(vm)));
}

SLOW_PATH_DECL(slow_path_in_by_val)
{
    BEGIN();
    auto bytecode = pc->as<OpInByVal>();
    auto& metadata = bytecode.metadata(exec);
    RETURN(jsBoolean(CommonSlowPaths::opInByVal(exec, GET_C(bytecode.m_base).jsValue(), GET_C(bytecode.m_property).jsValue(), &metadata.m_arrayProfile)));
}

SLOW_PATH_DECL(slow_path_in_by_id)
{
    BEGIN();

    auto bytecode = pc->as<OpInById>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    if (!baseValue.isObject())
        THROW(createInvalidInParameterError(exec, baseValue));

    RETURN(jsBoolean(asObject(baseValue)->hasProperty(exec, exec->codeBlock()->identifier(bytecode.m_property))));
}

SLOW_PATH_DECL(slow_path_del_by_val)
{
    BEGIN();
    auto bytecode = pc->as<OpDelByVal>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSObject* baseObject = baseValue.toObject(exec);
    CHECK_EXCEPTION();
    
    JSValue subscript = GET_C(bytecode.m_property).jsValue();
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable(vm)->deletePropertyByIndex(baseObject, exec, i);
    else {
        CHECK_EXCEPTION();
        auto property = subscript.toPropertyKey(exec);
        CHECK_EXCEPTION();
        couldDelete = baseObject->methodTable(vm)->deleteProperty(baseObject, exec, property);
    }
    
    if (!couldDelete && exec->codeBlock()->isStrictMode())
        THROW(createTypeError(exec, UnableToDeletePropertyError));
    
    RETURN(jsBoolean(couldDelete));
}

SLOW_PATH_DECL(slow_path_strcat)
{
    BEGIN();
    auto bytecode = pc->as<OpStrcat>();
    RETURN(jsStringFromRegisterArray(exec, &GET(bytecode.m_src), bytecode.m_count));
}

SLOW_PATH_DECL(slow_path_to_primitive)
{
    BEGIN();
    auto bytecode = pc->as<OpToPrimitive>();
    RETURN(GET_C(bytecode.m_src).jsValue().toPrimitive(exec));
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
    auto bytecode = pc->as<OpGetEnumerableLength>();
    JSValue enumeratorValue = GET(bytecode.m_base).jsValue();
    if (enumeratorValue.isUndefinedOrNull())
        RETURN(jsNumber(0));

    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(enumeratorValue.asCell());

    RETURN(jsNumber(enumerator->indexedLength()));
}

SLOW_PATH_DECL(slow_path_has_indexed_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasIndexedProperty>();
    auto& metadata = bytecode.metadata(exec);
    JSObject* base = GET(bytecode.m_base).jsValue().toObject(exec);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    metadata.m_arrayProfile.observeStructure(base->structure(vm));
    ASSERT(property.isUInt32());
    RETURN(jsBoolean(base->hasPropertyGeneric(exec, property.asUInt32(), PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_structure_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasStructureProperty>();
    JSObject* base = GET(bytecode.m_base).jsValue().toObject(exec);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    ASSERT(property.isString());
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    if (base->structure(vm)->id() == enumerator->cachedStructureID())
        RETURN(jsBoolean(true));
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(exec);
    CHECK_EXCEPTION();
    RETURN(jsBoolean(base->hasPropertyGeneric(exec, propertyName, PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_generic_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasGenericProperty>();
    JSObject* base = GET(bytecode.m_base).jsValue().toObject(exec);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    ASSERT(property.isString());
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(exec);
    CHECK_EXCEPTION();
    RETURN(jsBoolean(base->hasPropertyGeneric(exec, propertyName, PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_get_direct_pname)
{
    BEGIN();
    auto bytecode = pc->as<OpGetDirectPname>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue property = GET(bytecode.m_property).jsValue();
    ASSERT(property.isString());
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(exec);
    CHECK_EXCEPTION();
    RETURN(baseValue.get(exec, propertyName));
}

SLOW_PATH_DECL(slow_path_get_property_enumerator)
{
    BEGIN();
    auto bytecode = pc->as<OpGetPropertyEnumerator>();
    JSValue baseValue = GET(bytecode.m_base).jsValue();
    if (baseValue.isUndefinedOrNull())
        RETURN(JSPropertyNameEnumerator::create(vm));

    JSObject* base = baseValue.toObject(exec);
    CHECK_EXCEPTION();

    RETURN(propertyNameEnumerator(exec, base));
}

SLOW_PATH_DECL(slow_path_enumerator_structure_pname)
{
    BEGIN();
    auto bytecode = pc->as<OpEnumeratorStructurePname>();
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    uint32_t index = GET(bytecode.m_index).jsValue().asUInt32();

    JSString* propertyName = nullptr;
    if (index < enumerator->endStructurePropertyIndex())
        propertyName = enumerator->propertyNameAtIndex(index);
    RETURN(propertyName ? propertyName : jsNull());
}

SLOW_PATH_DECL(slow_path_enumerator_generic_pname)
{
    BEGIN();
    auto bytecode = pc->as<OpEnumeratorGenericPname>();
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    uint32_t index = GET(bytecode.m_index).jsValue().asUInt32();

    JSString* propertyName = nullptr;
    if (enumerator->endStructurePropertyIndex() <= index && index < enumerator->endGenericPropertyIndex())
        propertyName = enumerator->propertyNameAtIndex(index);
    RETURN(propertyName ? propertyName : jsNull());
}

SLOW_PATH_DECL(slow_path_to_index_string)
{
    BEGIN();
    auto bytecode = pc->as<OpToIndexString>();
    RETURN(jsString(exec, Identifier::from(exec, GET(bytecode.m_index).jsValue().asUInt32()).string()));
}

SLOW_PATH_DECL(slow_path_profile_type_clear_log)
{
    BEGIN();
    vm.typeProfilerLog()->processLogEntries(vm, "LLInt log full."_s);
    END();
}

SLOW_PATH_DECL(slow_path_unreachable)
{
    BEGIN();
    UNREACHABLE_FOR_PLATFORM();
    END();
}

SLOW_PATH_DECL(slow_path_create_lexical_environment)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateLexicalEnvironment>();
    int scopeReg = bytecode.m_scope.offset();
    JSScope* currentScope = exec->uncheckedR(scopeReg).Register::scope();
    SymbolTable* symbolTable = jsCast<SymbolTable*>(GET_C(bytecode.m_symbolTable).jsValue());
    JSValue initialValue = GET_C(bytecode.m_initialValue).jsValue();
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    JSScope* newScope = JSLexicalEnvironment::create(vm, exec->lexicalGlobalObject(), currentScope, symbolTable, initialValue);
    RETURN(newScope);
}

SLOW_PATH_DECL(slow_path_push_with_scope)
{
    BEGIN();
    auto bytecode = pc->as<OpPushWithScope>();
    JSObject* newScope = GET_C(bytecode.m_newScope).jsValue().toObject(exec);
    CHECK_EXCEPTION();

    int scopeReg = bytecode.m_currentScope.offset();
    JSScope* currentScope = exec->uncheckedR(scopeReg).Register::scope();
    RETURN(JSWithScope::create(vm, exec->lexicalGlobalObject(), currentScope, newScope));
}

SLOW_PATH_DECL(slow_path_resolve_scope_for_hoisting_func_decl_in_eval)
{
    BEGIN();
    auto bytecode = pc->as<OpResolveScopeForHoistingFuncDeclInEval>();
    const Identifier& ident = exec->codeBlock()->identifier(bytecode.m_property);
    JSScope* scope = exec->uncheckedR(bytecode.m_scope.offset()).Register::scope();
    JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(exec, scope, ident);

    CHECK_EXCEPTION();

    RETURN(resolvedScope);
}

SLOW_PATH_DECL(slow_path_resolve_scope)
{
    BEGIN();
    auto bytecode = pc->as<OpResolveScope>();
    auto& metadata = bytecode.metadata(exec);
    const Identifier& ident = exec->codeBlock()->identifier(bytecode.m_var);
    JSScope* scope = exec->uncheckedR(bytecode.m_scope.offset()).Register::scope();
    JSObject* resolvedScope = JSScope::resolve(exec, scope, ident);
    // Proxy can throw an error here, e.g. Proxy in with statement's @unscopables.
    CHECK_EXCEPTION();

    ResolveType resolveType = metadata.m_resolveType;

    // ModuleVar does not keep the scope register value alive in DFG.
    ASSERT(resolveType != ModuleVar);

    switch (resolveType) {
    case GlobalProperty:
    case GlobalPropertyWithVarInjectionChecks:
    case UnresolvedProperty:
    case UnresolvedPropertyWithVarInjectionChecks: {
        if (resolvedScope->isGlobalObject()) {
            JSGlobalObject* globalObject = jsCast<JSGlobalObject*>(resolvedScope);
            bool hasProperty = globalObject->hasProperty(exec, ident);
            CHECK_EXCEPTION();
            if (hasProperty) {
                ConcurrentJSLocker locker(exec->codeBlock()->m_lock);
                metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
                metadata.m_globalObject = globalObject;
                metadata.m_globalLexicalBindingEpoch = globalObject->globalLexicalBindingEpoch();
            }
        } else if (resolvedScope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(resolvedScope);
            ConcurrentJSLocker locker(exec->codeBlock()->m_lock);
            metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            metadata.m_globalLexicalEnvironment = globalLexicalEnvironment;
        }
        break;
    }
    default:
        break;
    }

    RETURN(resolvedScope);
}

SLOW_PATH_DECL(slow_path_create_rest)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateRest>();
    unsigned arraySize = GET_C(bytecode.m_arraySize).jsValue().asUInt32();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Structure* structure = globalObject->restParameterStructure();
    unsigned numParamsToSkip = bytecode.m_numParametersToSkip;
    JSValue* argumentsToCopyRegion = exec->addressOfArgumentsStart() + numParamsToSkip;
    RETURN(constructArray(exec, structure, argumentsToCopyRegion, arraySize));
}

SLOW_PATH_DECL(slow_path_get_by_id_with_this)
{
    BEGIN();
    auto bytecode = pc->as<OpGetByIdWithThis>();
    const Identifier& ident = exec->codeBlock()->identifier(bytecode.m_property);
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisVal = GET_C(bytecode.m_thisValue).jsValue();
    PropertySlot slot(thisVal, PropertySlot::PropertySlot::InternalMethodType::Get);
    JSValue result = baseValue.get(exec, ident, slot);
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_get_by_val_with_this)
{
    BEGIN();

    auto bytecode = pc->as<OpGetByValWithThis>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisValue = GET_C(bytecode.m_thisValue).jsValue();
    JSValue subscript = GET_C(bytecode.m_property).jsValue();

    if (LIKELY(baseValue.isCell() && subscript.isString())) {
        Structure& structure = *baseValue.asCell()->structure(vm);
        if (JSCell::canUseFastGetOwnProperty(structure)) {
            RefPtr<AtomicStringImpl> existingAtomicString = asString(subscript)->toExistingAtomicString(exec);
            CHECK_EXCEPTION();
            if (existingAtomicString) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomicString.get()))
                    RETURN_PROFILED(result);
            }
        }
    }
    
    PropertySlot slot(thisValue, PropertySlot::PropertySlot::InternalMethodType::Get);
    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            RETURN_PROFILED(asString(baseValue)->getIndex(exec, i));
        
        RETURN_PROFILED(baseValue.get(exec, i, slot));
    }

    baseValue.requireObjectCoercible(exec);
    CHECK_EXCEPTION();
    auto property = subscript.toPropertyKey(exec);
    CHECK_EXCEPTION();
    RETURN_PROFILED(baseValue.get(exec, property, slot));
}

SLOW_PATH_DECL(slow_path_put_by_id_with_this)
{
    BEGIN();
    auto bytecode = pc->as<OpPutByIdWithThis>();
    CodeBlock* codeBlock = exec->codeBlock();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisVal = GET_C(bytecode.m_thisValue).jsValue();
    JSValue putValue = GET_C(bytecode.m_value).jsValue();
    PutPropertySlot slot(thisVal, codeBlock->isStrictMode(), codeBlock->putByIdContext());
    baseValue.putInline(exec, ident, putValue, slot);
    END();
}

SLOW_PATH_DECL(slow_path_put_by_val_with_this)
{
    BEGIN();
    auto bytecode = pc->as<OpPutByValWithThis>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisValue = GET_C(bytecode.m_thisValue).jsValue();
    JSValue subscript = GET_C(bytecode.m_property).jsValue();
    JSValue value = GET_C(bytecode.m_value).jsValue();
    
    auto property = subscript.toPropertyKey(exec);
    CHECK_EXCEPTION();
    PutPropertySlot slot(thisValue, exec->codeBlock()->isStrictMode());
    baseValue.put(exec, property, value, slot);
    END();
}

SLOW_PATH_DECL(slow_path_define_data_property)
{
    BEGIN();
    auto bytecode = pc->as<OpDefineDataProperty>();
    JSObject* base = asObject(GET_C(bytecode.m_base).jsValue());
    JSValue property = GET_C(bytecode.m_property).jsValue();
    JSValue value = GET_C(bytecode.m_value).jsValue();
    JSValue attributes = GET_C(bytecode.m_attributes).jsValue();
    ASSERT(attributes.isInt32());

    auto propertyName = property.toPropertyKey(exec);
    CHECK_EXCEPTION();
    PropertyDescriptor descriptor = toPropertyDescriptor(value, jsUndefined(), jsUndefined(), DefinePropertyAttributes(attributes.asInt32()));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    base->methodTable(vm)->defineOwnProperty(base, exec, propertyName, descriptor, true);
    END();
}

SLOW_PATH_DECL(slow_path_define_accessor_property)
{
    BEGIN();
    auto bytecode = pc->as<OpDefineAccessorProperty>();
    JSObject* base = asObject(GET_C(bytecode.m_base).jsValue());
    JSValue property = GET_C(bytecode.m_property).jsValue();
    JSValue getter = GET_C(bytecode.m_getter).jsValue();
    JSValue setter = GET_C(bytecode.m_setter).jsValue();
    JSValue attributes = GET_C(bytecode.m_attributes).jsValue();
    ASSERT(attributes.isInt32());

    auto propertyName = property.toPropertyKey(exec);
    CHECK_EXCEPTION();
    PropertyDescriptor descriptor = toPropertyDescriptor(jsUndefined(), getter, setter, DefinePropertyAttributes(attributes.asInt32()));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    base->methodTable(vm)->defineOwnProperty(base, exec, propertyName, descriptor, true);
    END();
}

SLOW_PATH_DECL(slow_path_throw_static_error)
{
    BEGIN();
    auto bytecode = pc->as<OpThrowStaticError>();
    JSValue errorMessageValue = GET_C(bytecode.m_message).jsValue();
    RELEASE_ASSERT(errorMessageValue.isString());
    String errorMessage = asString(errorMessageValue)->value(exec);
    ErrorType errorType = bytecode.m_errorType;
    THROW(createError(exec, errorType, errorMessage));
}

SLOW_PATH_DECL(slow_path_new_array_with_spread)
{
    BEGIN();
    auto bytecode = pc->as<OpNewArrayWithSpread>();
    int numItems = bytecode.m_argc;
    ASSERT(numItems >= 0);
    const BitVector& bitVector = exec->codeBlock()->unlinkedCodeBlock()->bitVector(bytecode.m_bitVector);

    JSValue* values = bitwise_cast<JSValue*>(&GET(bytecode.m_argv));

    Checked<unsigned, RecordOverflow> checkedArraySize = 0;
    for (int i = 0; i < numItems; i++) {
        if (bitVector.get(i)) {
            JSValue value = values[-i];
            JSFixedArray* array = jsCast<JSFixedArray*>(value);
            checkedArraySize += array->size();
        } else
            checkedArraySize += 1;
    }
    if (UNLIKELY(checkedArraySize.hasOverflowed()))
        THROW(createOutOfMemoryError(exec));

    unsigned arraySize = checkedArraySize.unsafeGet();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

    JSArray* result = JSArray::tryCreate(vm, structure, arraySize);
    if (UNLIKELY(!result))
        THROW(createOutOfMemoryError(exec));
    CHECK_EXCEPTION();

    unsigned index = 0;
    for (int i = 0; i < numItems; i++) {
        JSValue value = values[-i];
        if (bitVector.get(i)) {
            // We are spreading.
            JSFixedArray* array = jsCast<JSFixedArray*>(value);
            for (unsigned i = 0; i < array->size(); i++) {
                RELEASE_ASSERT(array->get(i));
                result->putDirectIndex(exec, index, array->get(i));
                CHECK_EXCEPTION();
                ++index;
            }
        } else {
            // We are not spreading.
            result->putDirectIndex(exec, index, value);
            CHECK_EXCEPTION();
            ++index;
        }
    }

    RETURN(result);
}

SLOW_PATH_DECL(slow_path_new_array_buffer)
{
    BEGIN();
    auto bytecode = pc->as<OpNewArrayBuffer>();
    ASSERT(exec->codeBlock()->isConstantRegisterIndex(bytecode.m_immutableButterfly.offset()));
    JSImmutableButterfly* immutableButterfly = bitwise_cast<JSImmutableButterfly*>(GET_C(bytecode.m_immutableButterfly).jsValue().asCell());
    auto& profile = bytecode.metadata(exec).m_arrayAllocationProfile;

    IndexingType indexingMode = profile.selectIndexingType();
    Structure* structure = exec->lexicalGlobalObject()->arrayStructureForIndexingTypeDuringAllocation(indexingMode);
    ASSERT(isCopyOnWrite(indexingMode));
    ASSERT(!structure->outOfLineCapacity());

    if (UNLIKELY(immutableButterfly->indexingMode() != indexingMode)) {
        auto* newButterfly = JSImmutableButterfly::create(vm, indexingMode, immutableButterfly->length());
        for (unsigned i = 0; i < immutableButterfly->length(); ++i)
            newButterfly->setIndex(vm, i, immutableButterfly->get(i));
        immutableButterfly = newButterfly;
        CodeBlock* codeBlock = exec->codeBlock();

        // FIXME: This is kinda gross and only works because we can't inline new_array_bufffer in the baseline.
        // We also cannot allocate a new butterfly from compilation threads since it's invalid to allocate cells from
        // a compilation thread.
        WTF::storeStoreFence();
        codeBlock->constantRegister(bytecode.m_immutableButterfly.offset()).set(vm, codeBlock, immutableButterfly);
        WTF::storeStoreFence();
    }

    JSArray* result = CommonSlowPaths::allocateNewArrayBuffer(vm, structure, immutableButterfly);
    ASSERT(isCopyOnWrite(result->indexingMode()) || exec->lexicalGlobalObject()->isHavingABadTime());
    ArrayAllocationProfile::updateLastAllocationFor(&profile, result);
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_spread)
{
    BEGIN();

    auto bytecode = pc->as<OpSpread>();
    JSValue iterable = GET_C(bytecode.m_argument).jsValue();

    if (iterable.isCell() && isJSArray(iterable.asCell())) {
        JSArray* array = jsCast<JSArray*>(iterable);
        if (array->isIteratorProtocolFastAndNonObservable()) {
            // JSFixedArray::createFromArray does not consult the prototype chain,
            // so we must be sure that not consulting the prototype chain would
            // produce the same value during iteration.
            RETURN(JSFixedArray::createFromArray(exec, vm, array));
        }
    }

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    JSArray* array;
    {
        JSFunction* iterationFunction = globalObject->iteratorProtocolFunction();
        CallData callData;
        CallType callType = JSC::getCallData(vm, iterationFunction, callData);
        ASSERT(callType != CallType::None);

        MarkedArgumentBuffer arguments;
        arguments.append(iterable);
        ASSERT(!arguments.hasOverflowed());
        JSValue arrayResult = call(exec, iterationFunction, callType, callData, jsNull(), arguments);
        CHECK_EXCEPTION();
        array = jsCast<JSArray*>(arrayResult);
    }

    RETURN(JSFixedArray::createFromArray(exec, vm, array));
}

} // namespace JSC
