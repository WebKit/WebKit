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
#include "CommonSlowPaths.h"

#include "ArithProfile.h"
#include "BytecodeStructs.h"
#include "ClonedArguments.h"
#include "DefinePropertyAttributes.h"
#include "DirectArguments.h"
#include "ErrorHandlingScope.h"
#include "ExceptionFuzz.h"
#include "FrameTracers.h"
#include "JSArrayIterator.h"
#include "JSAsyncGenerator.h"
#include "JSCInlines.h"
#include "JSImmutableButterfly.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseConstructor.h"
#include "JSLexicalEnvironment.h"
#include "JSPromiseConstructor.h"
#include "JSPropertyNameEnumerator.h"
#include "JSWithScope.h"
#include "LLIntCommon.h"
#include "LLIntExceptions.h"
#include "MathCommon.h"
#include "ObjectConstructor.h"
#include "ScopedArguments.h"
#include "TypeProfilerLog.h"

namespace JSC {

#define BEGIN_NO_SET_PC() \
    CodeBlock* codeBlock = callFrame->codeBlock(); \
    JSGlobalObject* globalObject = codeBlock->globalObject(); \
    VM& vm = codeBlock->vm(); \
    SlowPathFrameTracer tracer(vm, callFrame); \
    auto throwScope = DECLARE_THROW_SCOPE(vm); \
    UNUSED_PARAM(throwScope)

#ifndef NDEBUG
#define SET_PC_FOR_STUBS() do { \
        codeBlock->bytecodeOffset(pc); \
        callFrame->setCurrentVPC(pc); \
    } while (false)
#else
#define SET_PC_FOR_STUBS() do { \
        callFrame->setCurrentVPC(pc); \
    } while (false)
#endif

#define RETURN_TO_THROW(pc)   pc = LLInt::returnToThrow(vm)

#define BEGIN()                           \
    BEGIN_NO_SET_PC();                    \
    SET_PC_FOR_STUBS()

#define GET(operand) (callFrame->uncheckedR(operand))
#define GET_C(operand) (callFrame->r(operand))

#define RETURN_TWO(first, second) do {       \
        return encodeResult(first, second);        \
    } while (false)

#define END_IMPL() RETURN_TWO(pc, callFrame)

#define THROW(exceptionToThrow) do {                        \
        throwException(globalObject, throwScope, exceptionToThrow); \
        RETURN_TO_THROW(pc);                          \
        END_IMPL();                                         \
    } while (false)

#define CHECK_EXCEPTION() do {                    \
        doExceptionFuzzingIfEnabled(globalObject, throwScope, "CommonSlowPaths", pc);   \
        if (UNLIKELY(throwScope.exception())) {   \
            RETURN_TO_THROW(pc);            \
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
                : codeBlock->outOfLineJumpTarget(pc);                              \
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

#define PROFILE_VALUE(value__) \
    PROFILE_VALUE_IN(value__, m_profile)

#define PROFILE_VALUE_IN(value, profileName) do { \
        bytecode.metadata(codeBlock).profileName.m_buckets[0] = JSValue::encode(value); \
    } while (false)

static void throwArityCheckStackOverflowError(JSGlobalObject* globalObject, ThrowScope& scope)
{
    JSObject* error = createStackOverflowError(globalObject);
    throwException(globalObject, scope, error);
#if LLINT_TRACING
    if (UNLIKELY(Options::traceLLIntSlowPath()))
        dataLog("Throwing exception ", JSValue(scope.exception()), ".\n");
#endif
}

SLOW_PATH_DECL(slow_path_call_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(vm, callFrame, CodeForCall);
    if (UNLIKELY(slotsToAdd < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(callFrame, CodeForCall);
        callFrame->convertToStackOverflowFrame(vm, codeBlock);
        SlowPathFrameTracer tracer(vm, callFrame);
        ErrorHandlingScope errorScope(vm);
        throwScope.release();
        throwArityCheckStackOverflowError(globalObject, throwScope);
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), callFrame);
    }
    RETURN_TWO(nullptr, bitwise_cast<void*>(static_cast<uintptr_t>(slotsToAdd)));
}

SLOW_PATH_DECL(slow_path_construct_arityCheck)
{
    BEGIN();
    int slotsToAdd = CommonSlowPaths::arityCheckFor(vm, callFrame, CodeForConstruct);
    if (UNLIKELY(slotsToAdd < 0)) {
        CodeBlock* codeBlock = CommonSlowPaths::codeBlockFromCallFrameCallee(callFrame, CodeForConstruct);
        callFrame->convertToStackOverflowFrame(vm, codeBlock);
        SlowPathFrameTracer tracer(vm, callFrame);
        ErrorHandlingScope errorScope(vm);
        throwArityCheckStackOverflowError(globalObject, throwScope);
        RETURN_TWO(bitwise_cast<void*>(static_cast<uintptr_t>(1)), callFrame);
    }
    RETURN_TWO(nullptr, bitwise_cast<void*>(static_cast<uintptr_t>(slotsToAdd)));
}

SLOW_PATH_DECL(slow_path_create_direct_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateDirectArguments>();
    RETURN(DirectArguments::createByCopying(globalObject, callFrame));
}

SLOW_PATH_DECL(slow_path_create_scoped_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateScopedArguments>();
    JSLexicalEnvironment* scope = jsCast<JSLexicalEnvironment*>(GET(bytecode.m_scope).jsValue());
    ScopedArgumentsTable* table = scope->symbolTable()->arguments();
    RETURN(ScopedArguments::createByCopying(globalObject, callFrame, table, scope));
}

SLOW_PATH_DECL(slow_path_create_cloned_arguments)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateClonedArguments>();
    RETURN(ClonedArguments::createWithMachineFrame(globalObject, callFrame, ArgumentsMode::Cloned));
}

SLOW_PATH_DECL(slow_path_create_arguments_butterfly)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateArgumentsButterfly>();
    int32_t argumentCount = callFrame->argumentCount();
    JSImmutableButterfly* butterfly = JSImmutableButterfly::tryCreate(vm, vm.immutableButterflyStructures[arrayIndexFromIndexingType(CopyOnWriteArrayWithContiguous) - NumberOfIndexingShapes].get(), argumentCount);
    if (!butterfly)
        THROW(createOutOfMemoryError(globalObject));
    for (int32_t index = 0; index < argumentCount; ++index)
        butterfly->setIndex(vm, index, callFrame->uncheckedArgument(index));
    RETURN(butterfly);
}

SLOW_PATH_DECL(slow_path_create_this)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateThis>();
    JSObject* result;
    JSObject* constructorAsObject = asObject(GET(bytecode.m_callee).jsValue());
    JSFunction* constructor = jsDynamicCast<JSFunction*>(vm, constructorAsObject);
    if (constructor && constructor->canUseAllocationProfile()) {
        WriteBarrier<JSCell>& cachedCallee = bytecode.metadata(codeBlock).m_cachedCallee;
        if (!cachedCallee)
            cachedCallee.set(vm, codeBlock, constructor);
        else if (cachedCallee.unvalidatedGet() != JSCell::seenMultipleCalleeObjects() && cachedCallee.get() != constructor)
            cachedCallee.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());

        size_t inlineCapacity = bytecode.m_inlineCapacity;
        ObjectAllocationProfileWithPrototype* allocationProfile = constructor->ensureRareDataAndAllocationProfile(globalObject, inlineCapacity)->objectAllocationProfile();
        throwScope.releaseAssertNoException();
        Structure* structure = allocationProfile->structure();
        result = constructEmptyObject(vm, structure);
        if (structure->hasPolyProto()) {
            JSObject* prototype = allocationProfile->prototype();
            ASSERT(prototype == constructor->prototypeForConstruction(vm, globalObject));
            result->putDirect(vm, knownPolyProtoOffset, prototype);
            prototype->didBecomePrototype();
            ASSERT_WITH_MESSAGE(!hasIndexedProperties(result->indexingType()), "We rely on JSFinalObject not starting out with an indexing type otherwise we would potentially need to convert to slow put storage");
        }
    } else {
        // http://ecma-international.org/ecma-262/6.0/#sec-ordinarycreatefromconstructor
        JSValue proto = constructorAsObject->get(globalObject, vm.propertyNames->prototype);
        CHECK_EXCEPTION();
        if (proto.isObject())
            result = constructEmptyObject(globalObject, asObject(proto));
        else
            result = constructEmptyObject(globalObject);
    }
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_create_promise)
{
    BEGIN();
    auto bytecode = pc->as<OpCreatePromise>();
    JSObject* constructorAsObject = asObject(GET(bytecode.m_callee).jsValue());

    JSPromise* result = nullptr;
    if (bytecode.m_isInternalPromise) {
        Structure* structure = constructorAsObject == globalObject->internalPromiseConstructor()
            ? globalObject->internalPromiseStructure()
            : InternalFunction::createSubclassStructure(globalObject, constructorAsObject, getFunctionRealm(vm, constructorAsObject)->internalPromiseStructure());
        CHECK_EXCEPTION();
        result = JSInternalPromise::create(vm, structure);
    } else {
        Structure* structure = constructorAsObject == globalObject->promiseConstructor()
            ? globalObject->promiseStructure()
            : InternalFunction::createSubclassStructure(globalObject, constructorAsObject, getFunctionRealm(vm, constructorAsObject)->promiseStructure());
        CHECK_EXCEPTION();
        result = JSPromise::create(vm, structure);
    }

    JSFunction* constructor = jsDynamicCast<JSFunction*>(vm, constructorAsObject);
    if (constructor && constructor->canUseAllocationProfile()) {
        WriteBarrier<JSCell>& cachedCallee = bytecode.metadata(codeBlock).m_cachedCallee;
        if (!cachedCallee)
            cachedCallee.set(vm, codeBlock, constructor);
        else if (cachedCallee.unvalidatedGet() != JSCell::seenMultipleCalleeObjects() && cachedCallee.get() != constructor)
            cachedCallee.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());
    }
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_new_promise)
{
    BEGIN();
    auto bytecode = pc->as<OpNewPromise>();
    JSPromise* result = nullptr;
    if (bytecode.m_isInternalPromise)
        result = JSInternalPromise::create(vm, globalObject->internalPromiseStructure());
    else
        result = JSPromise::create(vm, globalObject->promiseStructure());
    RETURN(result);
}

template<typename JSClass, typename Bytecode>
static JSClass* createInternalFieldObject(JSGlobalObject* globalObject, VM& vm, CodeBlock* codeBlock, const Bytecode& bytecode, JSObject* constructorAsObject, Structure* baseStructure)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    Structure* structure = InternalFunction::createSubclassStructure(globalObject, constructorAsObject, baseStructure);
    RETURN_IF_EXCEPTION(scope, nullptr);
    JSClass* result = JSClass::create(vm, structure);

    JSFunction* constructor = jsDynamicCast<JSFunction*>(vm, constructorAsObject);
    if (constructor && constructor->canUseAllocationProfile()) {
        WriteBarrier<JSCell>& cachedCallee = bytecode.metadata(codeBlock).m_cachedCallee;
        if (!cachedCallee)
            cachedCallee.set(vm, codeBlock, constructor);
        else if (cachedCallee.unvalidatedGet() != JSCell::seenMultipleCalleeObjects() && cachedCallee.get() != constructor)
            cachedCallee.setWithoutWriteBarrier(JSCell::seenMultipleCalleeObjects());
    }
    RELEASE_AND_RETURN(scope, result);
}

SLOW_PATH_DECL(slow_path_create_generator)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateGenerator>();
    RETURN(createInternalFieldObject<JSGenerator>(globalObject, vm, codeBlock, bytecode, asObject(GET(bytecode.m_callee).jsValue()), globalObject->generatorStructure()));
}

SLOW_PATH_DECL(slow_path_create_async_generator)
{
    BEGIN();
    auto bytecode = pc->as<OpCreateAsyncGenerator>();
    RETURN(createInternalFieldObject<JSAsyncGenerator>(globalObject, vm, codeBlock, bytecode, asObject(GET(bytecode.m_callee).jsValue()), globalObject->asyncGeneratorStructure()));
}

SLOW_PATH_DECL(slow_path_new_generator)
{
    BEGIN();
    auto bytecode = pc->as<OpNewGenerator>();
    JSGenerator* result = JSGenerator::create(vm, globalObject->generatorStructure());
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_to_this)
{
    BEGIN();
    auto bytecode = pc->as<OpToThis>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSValue v1 = GET(bytecode.m_srcDst).jsValue();
    if (v1.isCell()) {
        StructureID myStructureID = v1.asCell()->structureID();
        StructureID otherStructureID = metadata.m_cachedStructureID;
        if (myStructureID != otherStructureID) {
            if (otherStructureID)
                metadata.m_toThisStatus = ToThisConflicted;
            metadata.m_cachedStructureID = myStructureID;
            vm.heap.writeBarrier(codeBlock, vm.getStructure(myStructureID));
        }
    } else {
        metadata.m_toThisStatus = ToThisConflicted;
        metadata.m_cachedStructureID = 0;
    }
    // Note: We only need to do this value profiling here on the slow path. The fast path
    // just returns the input to to_this if the structure check succeeds. If the structure
    // check succeeds, doing value profiling here is equivalent to doing it with a potentially
    // different object that still has the same structure on the fast path since it'll produce
    // the same SpeculatedType. Therefore, we don't need to worry about value profiling on the
    // fast path.
    auto value = v1.toThis(globalObject, bytecode.m_ecmaMode);
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, value, PROFILE_VALUE(value));
}

SLOW_PATH_DECL(slow_path_throw_tdz_error)
{
    BEGIN();
    THROW(createTDZError(globalObject));
}

SLOW_PATH_DECL(slow_path_check_tdz)
{
    BEGIN();
    THROW(createTDZError(globalObject));
}

SLOW_PATH_DECL(slow_path_throw_strict_mode_readonly_property_write_error)
{
    BEGIN();
    THROW(createTypeError(globalObject, ReadonlyPropertyWriteError));
}

SLOW_PATH_DECL(slow_path_not)
{
    BEGIN();
    auto bytecode = pc->as<OpNot>();
    RETURN(jsBoolean(!GET_C(bytecode.m_operand).jsValue().toBoolean(globalObject)));
}

SLOW_PATH_DECL(slow_path_eq)
{
    BEGIN();
    auto bytecode = pc->as<OpEq>();
    RETURN(jsBoolean(JSValue::equal(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_neq)
{
    BEGIN();
    auto bytecode = pc->as<OpNeq>();
    RETURN(jsBoolean(!JSValue::equal(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_stricteq)
{
    BEGIN();
    auto bytecode = pc->as<OpStricteq>();
    RETURN(jsBoolean(JSValue::strictEqual(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_nstricteq)
{
    BEGIN();
    auto bytecode = pc->as<OpNstricteq>();
    RETURN(jsBoolean(!JSValue::strictEqual(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_less)
{
    BEGIN();
    auto bytecode = pc->as<OpLess>();
    RETURN(jsBoolean(jsLess<true>(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_lesseq)
{
    BEGIN();
    auto bytecode = pc->as<OpLesseq>();
    RETURN(jsBoolean(jsLessEq<true>(globalObject, GET_C(bytecode.m_lhs).jsValue(), GET_C(bytecode.m_rhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_greater)
{
    BEGIN();
    auto bytecode = pc->as<OpGreater>();
    RETURN(jsBoolean(jsLess<false>(globalObject, GET_C(bytecode.m_rhs).jsValue(), GET_C(bytecode.m_lhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_greatereq)
{
    BEGIN();
    auto bytecode = pc->as<OpGreatereq>();
    RETURN(jsBoolean(jsLessEq<false>(globalObject, GET_C(bytecode.m_rhs).jsValue(), GET_C(bytecode.m_lhs).jsValue())));
}

SLOW_PATH_DECL(slow_path_inc)
{
    BEGIN();
    auto bytecode = pc->as<OpInc>();
    JSValue argument = GET_C(bytecode.m_srcDst).jsValue();
    JSValue result = jsInc(globalObject, argument);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, result, { });
}

SLOW_PATH_DECL(slow_path_dec)
{
    BEGIN();
    auto bytecode = pc->as<OpDec>();
    JSValue argument = GET_C(bytecode.m_srcDst).jsValue();
    JSValue result = jsDec(globalObject, argument);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING_CUSTOM(bytecode.m_srcDst, result, { });
}

SLOW_PATH_DECL(slow_path_to_string)
{
    BEGIN();
    auto bytecode = pc->as<OpToString>();
    RETURN(GET_C(bytecode.m_operand).jsValue().toString(globalObject));
}

#if ENABLE(JIT)
static void updateArithProfileForUnaryArithOp(OpNegate::Metadata& metadata, JSValue result, JSValue operand)
{
    UnaryArithProfile& profile = metadata.m_arithProfile;
    profile.observeArg(operand);
    ASSERT(result.isNumber() || result.isBigInt());

    if (result.isHeapBigInt())
        profile.setObservedHeapBigInt();
#if USE(BIGINT32)
    else if (result.isBigInt32())
        profile.setObservedBigInt32();
#endif
    else {
        ASSERT(result.isNumber());
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
    }
}
#else
static void updateArithProfileForUnaryArithOp(OpNegate::Metadata&, JSValue, JSValue) { }
#endif

SLOW_PATH_DECL(slow_path_negate)
{
    BEGIN();
    auto bytecode = pc->as<OpNegate>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSValue operand = GET_C(bytecode.m_operand).jsValue();
    JSValue primValue = operand.toPrimitive(globalObject, PreferNumber);
    CHECK_EXCEPTION();

#if USE(BIGINT32)
    if (primValue.isBigInt32()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.bigInt32AsInt32());
        CHECK_EXCEPTION();
        RETURN_WITH_PROFILING(result, {
            updateArithProfileForUnaryArithOp(metadata, result, operand);
        });
    }
#endif

    if (primValue.isHeapBigInt()) {
        JSValue result = JSBigInt::unaryMinus(globalObject, primValue.asHeapBigInt());
        CHECK_EXCEPTION();
        RETURN_WITH_PROFILING(result, {
            updateArithProfileForUnaryArithOp(metadata, result, operand);
        });
    }
    
    JSValue result = jsNumber(-primValue.toNumber(globalObject));
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForUnaryArithOp(metadata, result, operand);
    });
}

#if ENABLE(DFG_JIT)
static void updateArithProfileForBinaryArithOp(JSGlobalObject*, CodeBlock* codeBlock, const Instruction* pc, JSValue result, JSValue left, JSValue right)
{
    BinaryArithProfile& profile = *codeBlock->binaryArithProfileForPC(pc);

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
    } else if (result.isHeapBigInt())
        profile.setObservedHeapBigInt();
#if USE(BIGINT32)
    else if (result.isBigInt32())
        profile.setObservedBigInt32();
#endif
    else 
        profile.setObservedNonNumeric();
}
#else
static void updateArithProfileForBinaryArithOp(JSGlobalObject*, CodeBlock*, const Instruction*, JSValue, JSValue, JSValue) { }
#endif

SLOW_PATH_DECL(slow_path_to_number)
{
    BEGIN();
    auto bytecode = pc->as<OpToNumber>();
    JSValue argument = GET_C(bytecode.m_operand).jsValue();
    JSValue result = jsNumber(argument.toNumber(globalObject));
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_to_numeric)
{
    BEGIN();
    auto bytecode = pc->as<OpToNumeric>();
    JSValue argument = GET_C(bytecode.m_operand).jsValue();
    JSValue result = argument.toNumeric(globalObject);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_to_object)
{
    BEGIN();
    auto bytecode = pc->as<OpToObject>();
    JSValue argument = GET_C(bytecode.m_operand).jsValue();
    if (UNLIKELY(argument.isUndefinedOrNull())) {
        const Identifier& ident = codeBlock->identifier(bytecode.m_message);
        if (!ident.isEmpty())
            THROW(createTypeError(globalObject, ident.impl()));
    }
    JSObject* result = argument.toObject(globalObject);
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_add)
{
    BEGIN();
    auto bytecode = pc->as<OpAdd>();
    JSValue v1 = GET_C(bytecode.m_lhs).jsValue();
    JSValue v2 = GET_C(bytecode.m_rhs).jsValue();

    BinaryArithProfile& arithProfile = *codeBlock->binaryArithProfileForPC(pc);
    arithProfile.observeLHSAndRHS(v1, v2);

    JSValue result = jsAdd(globalObject, v1, v2);

    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(globalObject, codeBlock, pc, result, v1, v2);
    });
}

// The following arithmetic and bitwise operations need to be sure to run
// toNumeric/toBigIntOrInt32() on their operands in order.  (A call to these is idempotent
// if an exception is already set on the CallFrame.)

SLOW_PATH_DECL(slow_path_mul)
{
    BEGIN();
    auto bytecode = pc->as<OpMul>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsMul(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(globalObject, codeBlock, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_sub)
{
    BEGIN();
    auto bytecode = pc->as<OpSub>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsSub(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(globalObject, codeBlock, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_div)
{
    BEGIN();
    auto bytecode = pc->as<OpDiv>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsDiv(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_WITH_PROFILING(result, {
        updateArithProfileForBinaryArithOp(globalObject, codeBlock, pc, result, left, right);
    });
}

SLOW_PATH_DECL(slow_path_mod)
{
    BEGIN();
    auto bytecode = pc->as<OpMod>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsRemainder(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_pow)
{
    BEGIN();
    auto bytecode = pc->as<OpPow>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();
    JSValue result = jsPow(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_lshift)
{
    BEGIN();
    auto bytecode = pc->as<OpLshift>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsLShift(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_rshift)
{
    BEGIN();
    auto bytecode = pc->as<OpRshift>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsRShift(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_urshift)
{
    BEGIN();
    auto bytecode = pc->as<OpUrshift>();
    JSValue left = GET_C(bytecode.m_lhs).jsValue();
    JSValue right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsURShift(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN(result);
}

SLOW_PATH_DECL(slow_path_unsigned)
{
    BEGIN();
    auto bytecode = pc->as<OpUnsigned>();
    uint32_t a = GET_C(bytecode.m_operand).jsValue().toUInt32(globalObject);
    RETURN(jsNumber(a));
}

SLOW_PATH_DECL(slow_path_bitnot)
{
    BEGIN();
    auto bytecode = pc->as<OpBitnot>();
    auto operand = GET_C(bytecode.m_operand).jsValue();

    auto result = jsBitwiseNot(globalObject, operand);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_bitand)
{
    BEGIN();
    auto bytecode = pc->as<OpBitand>();
    auto left = GET_C(bytecode.m_lhs).jsValue();
    auto right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsBitwiseAnd(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_bitor)
{
    BEGIN();
    auto bytecode = pc->as<OpBitor>();
    auto left = GET_C(bytecode.m_lhs).jsValue();
    auto right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsBitwiseOr(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_bitxor)
{
    BEGIN();
    auto bytecode = pc->as<OpBitxor>();
    auto left = GET_C(bytecode.m_lhs).jsValue();
    auto right = GET_C(bytecode.m_rhs).jsValue();

    JSValue result = jsBitwiseXor(globalObject, left, right);
    CHECK_EXCEPTION();
    RETURN_PROFILED(result);
}

SLOW_PATH_DECL(slow_path_typeof)
{
    BEGIN();
    auto bytecode = pc->as<OpTypeof>();
    RETURN(jsTypeStringForValue(globalObject, GET_C(bytecode.m_value).jsValue()));
}

SLOW_PATH_DECL(slow_path_is_object_or_null)
{
    BEGIN();
    auto bytecode = pc->as<OpIsObjectOrNull>();
    RETURN(jsBoolean(jsIsObjectTypeOrNull(globalObject, GET_C(bytecode.m_operand).jsValue())));
}

SLOW_PATH_DECL(slow_path_is_function)
{
    BEGIN();
    auto bytecode = pc->as<OpIsFunction>();
    RETURN(jsBoolean(GET_C(bytecode.m_operand).jsValue().isCallable(vm)));
}

SLOW_PATH_DECL(slow_path_is_constructor)
{
    BEGIN();
    auto bytecode = pc->as<OpIsConstructor>();
    RETURN(jsBoolean(GET_C(bytecode.m_operand).jsValue().isConstructor(vm)));
}

SLOW_PATH_DECL(slow_path_in_by_val)
{
    BEGIN();
    auto bytecode = pc->as<OpInByVal>();
    auto& metadata = bytecode.metadata(codeBlock);
    RETURN(jsBoolean(CommonSlowPaths::opInByVal(globalObject, GET_C(bytecode.m_base).jsValue(), GET_C(bytecode.m_property).jsValue(), &metadata.m_arrayProfile)));
}

SLOW_PATH_DECL(slow_path_in_by_id)
{
    BEGIN();

    auto bytecode = pc->as<OpInById>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    if (!baseValue.isObject())
        THROW(createInvalidInParameterError(globalObject, baseValue));

    RETURN(jsBoolean(asObject(baseValue)->hasProperty(globalObject, codeBlock->identifier(bytecode.m_property))));
}

template<OpcodeSize width>
SlowPathReturnType SLOW_PATH iterator_open_try_fast(CallFrame* callFrame, const Instruction* pc)
{
    // Don't set PC; we can't throw and it's relatively slow.
    BEGIN_NO_SET_PC();

    auto bytecode = pc->asKnownWidth<OpIteratorOpen, width>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSValue iterable = GET_C(bytecode.m_iterable).jsValue();
    PROFILE_VALUE_IN(iterable, m_iterableProfile);
    JSValue symbolIterator = GET_C(bytecode.m_symbolIterator).jsValue();
    auto& iterator = GET(bytecode.m_iterator);

    auto prepareForFastArrayIteration = [&] {
        if (!globalObject->arrayIteratorProtocolWatchpointSet().isStillValid())
            return IterationMode::Generic;

        // This is correct because we just checked the watchpoint is still valid.
        JSFunction* symbolIteratorFunction = jsDynamicCast<JSFunction*>(vm, symbolIterator);
        if (!symbolIteratorFunction)
            return IterationMode::Generic; 

        // We don't want to allocate the values function just to check if it's the same as our function at so we use the concurrent accessor.
        // FIXME: This only works for arrays from the same global object as ourselves but we should be able to support any pairing.
        if (globalObject->arrayProtoValuesFunctionConcurrently() != symbolIteratorFunction)
            return IterationMode::Generic;

        // We should be good to go.
        metadata.m_iterationMetadata.seenModes = metadata.m_iterationMetadata.seenModes | IterationMode::FastArray;
        GET(bytecode.m_next) = JSValue();
        auto* iteratedObject = jsCast<JSObject*>(iterable);
        iterator = JSArrayIterator::create(vm, globalObject->arrayIteratorStructure(), iteratedObject, IterationKind::Values);
        PROFILE_VALUE_IN(iterator.jsValue(), m_iteratorProfile);
        return IterationMode::FastArray;
    };

    if (iterable.inherits<JSArray>(vm)) {
        if (prepareForFastArrayIteration() == IterationMode::FastArray)
            return encodeResult(pc, reinterpret_cast<void*>(IterationMode::FastArray));
    }

    // Return to the bytecode to try in generic mode.
    metadata.m_iterationMetadata.seenModes = metadata.m_iterationMetadata.seenModes | IterationMode::Generic;
    return encodeResult(pc, reinterpret_cast<void*>(IterationMode::Generic));
}

SLOW_PATH_DECL(iterator_open_try_fast_narrow)
{
    return iterator_open_try_fast<Narrow>(callFrame, pc);
}

SLOW_PATH_DECL(iterator_open_try_fast_wide16)
{
    return iterator_open_try_fast<Wide16>(callFrame, pc);
}

SLOW_PATH_DECL(iterator_open_try_fast_wide32)
{
    return iterator_open_try_fast<Wide32>(callFrame, pc);
}

template<OpcodeSize width>
SlowPathReturnType SLOW_PATH iterator_next_try_fast(CallFrame* callFrame, const Instruction* pc)
{
    BEGIN();

    auto bytecode = pc->asKnownWidth<OpIteratorNext, width>();
    auto& metadata = bytecode.metadata(codeBlock);

    ASSERT(!GET(bytecode.m_next).jsValue());
    JSObject* iterator = jsCast<JSObject*>(GET(bytecode.m_iterator).jsValue());;
    JSCell* iterable = GET(bytecode.m_iterable).jsValue().asCell();
    if (auto arrayIterator = jsDynamicCast<JSArrayIterator*>(vm, iterator)) {
        if (auto array = jsDynamicCast<JSArray*>(vm, iterable)) {
            metadata.m_iterableProfile.observeStructureID(array->structureID());

            metadata.m_iterationMetadata.seenModes = metadata.m_iterationMetadata.seenModes | IterationMode::FastArray;
            auto& indexSlot = arrayIterator->internalField(JSArrayIterator::Field::Index);
            int64_t index = indexSlot.get().asAnyInt();
            ASSERT(0 <= index && index <= maxSafeInteger());

            JSValue value;
            bool done = index == JSArrayIterator::doneIndex || index >= array->length();
            GET(bytecode.m_done) = jsBoolean(done);
            if (!done) {
                // No need for a barrier here because we know this is a primitive.
                indexSlot.setWithoutWriteBarrier(jsNumber(index + 1));
                ASSERT(index == static_cast<unsigned>(index));
                value = array->getIndex(globalObject, static_cast<unsigned>(index));
                CHECK_EXCEPTION();
                PROFILE_VALUE_IN(value, m_valueProfile);
            } else {
                // No need for a barrier here because we know this is a primitive.
                indexSlot.setWithoutWriteBarrier(jsNumber(-1));
            }

            GET(bytecode.m_value) = value;
            return encodeResult(pc, reinterpret_cast<void*>(IterationMode::FastArray));
        }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

SLOW_PATH_DECL(iterator_next_try_fast_narrow)
{
    return iterator_next_try_fast<Narrow>(callFrame, pc);
}

SLOW_PATH_DECL(iterator_next_try_fast_wide16)
{
    return iterator_next_try_fast<Wide16>(callFrame, pc);
}

SLOW_PATH_DECL(iterator_next_try_fast_wide32)
{
    return iterator_next_try_fast<Wide32>(callFrame, pc);
}

SLOW_PATH_DECL(slow_path_del_by_val)
{
    BEGIN();
    auto bytecode = pc->as<OpDelByVal>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSObject* baseObject = baseValue.toObject(globalObject);
    CHECK_EXCEPTION();
    
    JSValue subscript = GET_C(bytecode.m_property).jsValue();
    
    bool couldDelete;
    
    uint32_t i;
    if (subscript.getUInt32(i))
        couldDelete = baseObject->methodTable(vm)->deletePropertyByIndex(baseObject, globalObject, i);
    else {
        CHECK_EXCEPTION();
        auto property = subscript.toPropertyKey(globalObject);
        CHECK_EXCEPTION();
        couldDelete = JSCell::deleteProperty(baseObject, globalObject, property);
    }
    CHECK_EXCEPTION();
    
    if (!couldDelete && bytecode.m_ecmaMode.isStrict())
        THROW(createTypeError(globalObject, UnableToDeletePropertyError));
    
    RETURN(jsBoolean(couldDelete));
}

SLOW_PATH_DECL(slow_path_strcat)
{
    BEGIN();
    auto bytecode = pc->as<OpStrcat>();
    RETURN(jsStringFromRegisterArray(globalObject, &GET(bytecode.m_src), bytecode.m_count));
}

SLOW_PATH_DECL(slow_path_to_primitive)
{
    BEGIN();
    auto bytecode = pc->as<OpToPrimitive>();
    RETURN(GET_C(bytecode.m_src).jsValue().toPrimitive(globalObject));
}

SLOW_PATH_DECL(slow_path_enter)
{
    BEGIN();
    Heap::heap(codeBlock)->writeBarrier(codeBlock);
    END();
}

SLOW_PATH_DECL(slow_path_to_property_key)
{
    BEGIN();
    auto bytecode = pc->as<OpToPropertyKey>();
    RETURN(GET_C(bytecode.m_src).jsValue().toPropertyKeyValue(globalObject));
}

SLOW_PATH_DECL(slow_path_get_enumerable_length)
{
    BEGIN();
    auto bytecode = pc->as<OpGetEnumerableLength>();
    JSValue enumeratorValue = GET_C(bytecode.m_base).jsValue();
    if (enumeratorValue.isUndefinedOrNull())
        RETURN(jsNumber(0));

    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(enumeratorValue.asCell());

    RETURN(jsNumber(enumerator->indexedLength()));
}

SLOW_PATH_DECL(slow_path_has_indexed_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasIndexedProperty>();
    auto& metadata = bytecode.metadata(codeBlock);
    JSObject* base = GET_C(bytecode.m_base).jsValue().toObject(globalObject);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    metadata.m_arrayProfile.observeStructure(base->structure(vm));
    ASSERT(property.isUInt32AsAnyInt());
    RETURN(jsBoolean(base->hasPropertyGeneric(globalObject, property.asUInt32AsAnyInt(), PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_structure_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasStructureProperty>();
    JSObject* base = GET_C(bytecode.m_base).jsValue().toObject(globalObject);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    RELEASE_ASSERT(property.isString());
#if USE(JSVALUE32_64)
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    if (base->structure(vm)->id() == enumerator->cachedStructureID())
        RETURN(jsBoolean(true));
#endif
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(globalObject);
    CHECK_EXCEPTION();
    RETURN(jsBoolean(base->hasPropertyGeneric(globalObject, propertyName, PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_has_own_structure_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasOwnStructureProperty>();
    JSValue base = GET_C(bytecode.m_base).jsValue();

#if USE(JSVALUE32_64)
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    if (base.isCell() && base.asCell()->structure(vm)->id() == enumerator->cachedStructureID())
        RETURN(jsBoolean(true));
#endif

    JSValue property = GET(bytecode.m_property).jsValue();
    RELEASE_ASSERT(property.isString());
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(globalObject);
    CHECK_EXCEPTION();

    RETURN(jsBoolean(objectPrototypeHasOwnProperty(globalObject, base, propertyName)));
}

SLOW_PATH_DECL(slow_path_in_structure_property)
{
    BEGIN();
    auto bytecode = pc->as<OpInStructureProperty>();
    JSValue base = GET_C(bytecode.m_base).jsValue();
#if USE(JSVALUE32_64)
    JSPropertyNameEnumerator* enumerator = jsCast<JSPropertyNameEnumerator*>(GET(bytecode.m_enumerator).jsValue().asCell());
    if (base.isCell() && base.asCell()->structure(vm)->id() == enumerator->cachedStructureID())
        RETURN(jsBoolean(true));
#endif
    JSValue property = GET(bytecode.m_property).jsValue();
    RELEASE_ASSERT(property.isString());
    RETURN(jsBoolean(CommonSlowPaths::opInByVal(globalObject, base, asString(property))));
}

SLOW_PATH_DECL(slow_path_has_generic_property)
{
    BEGIN();
    auto bytecode = pc->as<OpHasGenericProperty>();
    JSObject* base = GET_C(bytecode.m_base).jsValue().toObject(globalObject);
    CHECK_EXCEPTION();
    JSValue property = GET(bytecode.m_property).jsValue();
    ASSERT(property.isString());
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(globalObject);
    CHECK_EXCEPTION();
    RETURN(jsBoolean(base->hasPropertyGeneric(globalObject, propertyName, PropertySlot::InternalMethodType::GetOwnProperty)));
}

SLOW_PATH_DECL(slow_path_get_direct_pname)
{
    BEGIN();
    auto bytecode = pc->as<OpGetDirectPname>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue property = GET(bytecode.m_property).jsValue();
    ASSERT(property.isString());
    JSString* string = asString(property);
    auto propertyName = string->toIdentifier(globalObject);
    CHECK_EXCEPTION();
    RETURN(baseValue.get(globalObject, propertyName));
}

SLOW_PATH_DECL(slow_path_get_property_enumerator)
{
    BEGIN();
    auto bytecode = pc->as<OpGetPropertyEnumerator>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    if (baseValue.isUndefinedOrNull())
        RETURN(vm.emptyPropertyNameEnumerator());

    JSObject* base = baseValue.toObject(globalObject);
    CHECK_EXCEPTION();

    RETURN(propertyNameEnumerator(globalObject, base));
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
    JSValue indexValue = GET(bytecode.m_index).jsValue();
    ASSERT(indexValue.isUInt32AsAnyInt());
    RETURN(jsString(vm, Identifier::from(vm, indexValue.asUInt32AsAnyInt()).string()));
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
    JSScope* currentScope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    SymbolTable* symbolTable = jsCast<SymbolTable*>(GET_C(bytecode.m_symbolTable).jsValue());
    JSValue initialValue = GET_C(bytecode.m_initialValue).jsValue();
    ASSERT(initialValue == jsUndefined() || initialValue == jsTDZValue());
    JSScope* newScope = JSLexicalEnvironment::create(vm, globalObject, currentScope, symbolTable, initialValue);
    RETURN(newScope);
}

SLOW_PATH_DECL(slow_path_push_with_scope)
{
    BEGIN();
    auto bytecode = pc->as<OpPushWithScope>();
    JSObject* newScope = GET_C(bytecode.m_newScope).jsValue().toObject(globalObject);
    CHECK_EXCEPTION();

    JSScope* currentScope = callFrame->uncheckedR(bytecode.m_currentScope).Register::scope();
    RETURN(JSWithScope::create(vm, globalObject, currentScope, newScope));
}

SLOW_PATH_DECL(slow_path_resolve_scope_for_hoisting_func_decl_in_eval)
{
    BEGIN();
    auto bytecode = pc->as<OpResolveScopeForHoistingFuncDeclInEval>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    JSValue resolvedScope = JSScope::resolveScopeForHoistingFuncDeclInEval(globalObject, scope, ident);

    CHECK_EXCEPTION();

    RETURN(resolvedScope);
}

SLOW_PATH_DECL(slow_path_resolve_scope)
{
    BEGIN();
    auto bytecode = pc->as<OpResolveScope>();
    auto& metadata = bytecode.metadata(codeBlock);
    const Identifier& ident = codeBlock->identifier(bytecode.m_var);
    JSScope* scope = callFrame->uncheckedR(bytecode.m_scope).Register::scope();
    JSObject* resolvedScope = JSScope::resolve(globalObject, scope, ident);
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
            bool hasProperty = globalObject->hasProperty(globalObject, ident);
            CHECK_EXCEPTION();
            if (hasProperty) {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalPropertyWithVarInjectionChecks : GlobalProperty;
                metadata.m_globalObject.set(vm, codeBlock, globalObject);
                metadata.m_globalLexicalBindingEpoch = globalObject->globalLexicalBindingEpoch();
            }
        } else if (resolvedScope->isGlobalLexicalEnvironment()) {
            JSGlobalLexicalEnvironment* globalLexicalEnvironment = jsCast<JSGlobalLexicalEnvironment*>(resolvedScope);
            ConcurrentJSLocker locker(codeBlock->m_lock);
            metadata.m_resolveType = needsVarInjectionChecks(resolveType) ? GlobalLexicalVarWithVarInjectionChecks : GlobalLexicalVar;
            metadata.m_globalLexicalEnvironment.set(vm, codeBlock, globalLexicalEnvironment);
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
    Structure* structure = globalObject->restParameterStructure();
    unsigned numParamsToSkip = bytecode.m_numParametersToSkip;
    JSValue* argumentsToCopyRegion = callFrame->addressOfArgumentsStart() + numParamsToSkip;
    RETURN(constructArray(globalObject, structure, argumentsToCopyRegion, arraySize));
}

SLOW_PATH_DECL(slow_path_get_by_id_with_this)
{
    BEGIN();
    auto bytecode = pc->as<OpGetByIdWithThis>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisVal = GET_C(bytecode.m_thisValue).jsValue();
    PropertySlot slot(thisVal, PropertySlot::PropertySlot::InternalMethodType::Get);
    JSValue result = baseValue.get(globalObject, ident, slot);
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
            RefPtr<AtomStringImpl> existingAtomString = asString(subscript)->toExistingAtomString(globalObject);
            CHECK_EXCEPTION();
            if (existingAtomString) {
                if (JSValue result = baseValue.asCell()->fastGetOwnProperty(vm, structure, existingAtomString.get()))
                    RETURN_PROFILED(result);
            }
        }
    }
    
    PropertySlot slot(thisValue, PropertySlot::PropertySlot::InternalMethodType::Get);
    if (subscript.isUInt32()) {
        uint32_t i = subscript.asUInt32();
        if (isJSString(baseValue) && asString(baseValue)->canGetIndex(i))
            RETURN_PROFILED(asString(baseValue)->getIndex(globalObject, i));
        
        RETURN_PROFILED(baseValue.get(globalObject, i, slot));
    }

    baseValue.requireObjectCoercible(globalObject);
    CHECK_EXCEPTION();
    auto property = subscript.toPropertyKey(globalObject);
    CHECK_EXCEPTION();
    RETURN_PROFILED(baseValue.get(globalObject, property, slot));
}

SLOW_PATH_DECL(slow_path_get_private_name)
{
    BEGIN();

    auto bytecode = pc->as<OpGetPrivateName>();
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue subscript = GET_C(bytecode.m_property).jsValue();
    ASSERT(subscript.isSymbol());

    baseValue.requireObjectCoercible(globalObject);
    CHECK_EXCEPTION();
    auto property = subscript.toPropertyKey(globalObject);
    CHECK_EXCEPTION();
    ASSERT(property.isPrivateName());

    PropertySlot slot(baseValue, PropertySlot::InternalMethodType::GetOwnProperty);
    asObject(baseValue)->getPrivateField(globalObject, property, slot);
    CHECK_EXCEPTION();

    RETURN_PROFILED(slot.getValue(globalObject, property));
}

SLOW_PATH_DECL(slow_path_get_prototype_of)
{
    BEGIN();
    auto bytecode = pc->as<OpGetPrototypeOf>();
    JSValue value = GET_C(bytecode.m_value).jsValue();
    RETURN_PROFILED(value.getPrototype(globalObject));
}

SLOW_PATH_DECL(slow_path_put_by_id_with_this)
{
    BEGIN();
    auto bytecode = pc->as<OpPutByIdWithThis>();
    const Identifier& ident = codeBlock->identifier(bytecode.m_property);
    JSValue baseValue = GET_C(bytecode.m_base).jsValue();
    JSValue thisVal = GET_C(bytecode.m_thisValue).jsValue();
    JSValue putValue = GET_C(bytecode.m_value).jsValue();
    PutPropertySlot slot(thisVal, bytecode.m_ecmaMode.isStrict(), codeBlock->putByIdContext());
    baseValue.putInline(globalObject, ident, putValue, slot);
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
    
    auto property = subscript.toPropertyKey(globalObject);
    CHECK_EXCEPTION();
    PutPropertySlot slot(thisValue, bytecode.m_ecmaMode.isStrict());
    baseValue.put(globalObject, property, value, slot);
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

    auto propertyName = property.toPropertyKey(globalObject);
    CHECK_EXCEPTION();
    PropertyDescriptor descriptor = toPropertyDescriptor(value, jsUndefined(), jsUndefined(), DefinePropertyAttributes(attributes.asInt32()));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    base->methodTable(vm)->defineOwnProperty(base, globalObject, propertyName, descriptor, true);
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

    auto propertyName = property.toPropertyKey(globalObject);
    CHECK_EXCEPTION();
    PropertyDescriptor descriptor = toPropertyDescriptor(jsUndefined(), getter, setter, DefinePropertyAttributes(attributes.asInt32()));
    ASSERT((descriptor.attributes() & PropertyAttribute::Accessor) || (!descriptor.isAccessorDescriptor()));
    base->methodTable(vm)->defineOwnProperty(base, globalObject, propertyName, descriptor, true);
    END();
}

SLOW_PATH_DECL(slow_path_throw_static_error)
{
    BEGIN();
    auto bytecode = pc->as<OpThrowStaticError>();
    JSValue errorMessageValue = GET_C(bytecode.m_message).jsValue();
    RELEASE_ASSERT(errorMessageValue.isString());
    String errorMessage = asString(errorMessageValue)->value(globalObject);
    ErrorTypeWithExtension errorType = bytecode.m_errorType;
    THROW(createError(globalObject, errorType, errorMessage));
}

SLOW_PATH_DECL(slow_path_new_array_with_spread)
{
    BEGIN();
    auto bytecode = pc->as<OpNewArrayWithSpread>();
    int numItems = bytecode.m_argc;
    ASSERT(numItems >= 0);
    const BitVector& bitVector = codeBlock->unlinkedCodeBlock()->bitVector(bytecode.m_bitVector);

    JSValue* values = bitwise_cast<JSValue*>(&GET(bytecode.m_argv));

    if (numItems == 1 && bitVector.get(0)) {
        Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(CopyOnWriteArrayWithContiguous);
        if (isCopyOnWrite(structure->indexingMode())) {
            JSArray* result = CommonSlowPaths::allocateNewArrayBuffer(vm, structure, jsCast<JSImmutableButterfly*>(values[0]));
            RETURN(result);
        }
    }

    Checked<unsigned, RecordOverflow> checkedArraySize = 0;
    for (int i = 0; i < numItems; i++) {
        if (bitVector.get(i)) {
            JSValue value = values[-i];
            JSImmutableButterfly* array = jsCast<JSImmutableButterfly*>(value);
            checkedArraySize += array->publicLength();
        } else
            checkedArraySize += 1;
    }
    if (UNLIKELY(checkedArraySize.hasOverflowed()))
        THROW(createOutOfMemoryError(globalObject));

    unsigned arraySize = checkedArraySize.unsafeGet();
    if (UNLIKELY(arraySize >= MIN_ARRAY_STORAGE_CONSTRUCTION_LENGTH))
        THROW(createOutOfMemoryError(globalObject));

    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous);

    JSArray* result = JSArray::tryCreate(vm, structure, arraySize);
    if (UNLIKELY(!result))
        THROW(createOutOfMemoryError(globalObject));
    CHECK_EXCEPTION();

    unsigned index = 0;
    for (int i = 0; i < numItems; i++) {
        JSValue value = values[-i];
        if (bitVector.get(i)) {
            // We are spreading.
            JSImmutableButterfly* array = jsCast<JSImmutableButterfly*>(value);
            for (unsigned i = 0; i < array->publicLength(); i++) {
                RELEASE_ASSERT(array->get(i));
                result->putDirectIndex(globalObject, index, array->get(i));
                CHECK_EXCEPTION();
                ++index;
            }
        } else {
            // We are not spreading.
            result->putDirectIndex(globalObject, index, value);
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
    ASSERT(bytecode.m_immutableButterfly.isConstant());
    JSImmutableButterfly* immutableButterfly = bitwise_cast<JSImmutableButterfly*>(GET_C(bytecode.m_immutableButterfly).jsValue().asCell());
    auto& profile = bytecode.metadata(codeBlock).m_arrayAllocationProfile;

    IndexingType indexingMode = profile.selectIndexingType();
    Structure* structure = globalObject->arrayStructureForIndexingTypeDuringAllocation(indexingMode);
    ASSERT(isCopyOnWrite(indexingMode));
    ASSERT(!structure->outOfLineCapacity());

    if (UNLIKELY(immutableButterfly->indexingMode() != indexingMode)) {
        auto* newButterfly = JSImmutableButterfly::create(vm, indexingMode, immutableButterfly->length());
        for (unsigned i = 0; i < immutableButterfly->length(); ++i)
            newButterfly->setIndex(vm, i, immutableButterfly->get(i));
        immutableButterfly = newButterfly;

        // FIXME: This is kinda gross and only works because we can't inline new_array_bufffer in the baseline.
        // We also cannot allocate a new butterfly from compilation threads since it's invalid to allocate cells from
        // a compilation thread.
        WTF::storeStoreFence();
        codeBlock->constantRegister(bytecode.m_immutableButterfly).set(vm, codeBlock, immutableButterfly);
        WTF::storeStoreFence();
    }

    JSArray* result = CommonSlowPaths::allocateNewArrayBuffer(vm, structure, immutableButterfly);
    ASSERT(isCopyOnWrite(result->indexingMode()) || globalObject->isHavingABadTime());
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
            // JSImmutableButterfly::createFromArray does not consult the prototype chain,
            // so we must be sure that not consulting the prototype chain would
            // produce the same value during iteration.
            RETURN(JSImmutableButterfly::createFromArray(globalObject, vm, array));
        }
    }

    JSArray* array;
    {
        JSFunction* iterationFunction = globalObject->iteratorProtocolFunction();
        auto callData = getCallData(vm, iterationFunction);
        ASSERT(callData.type != CallData::Type::None);

        MarkedArgumentBuffer arguments;
        arguments.append(iterable);
        ASSERT(!arguments.hasOverflowed());
        JSValue arrayResult = call(globalObject, iterationFunction, callData, jsNull(), arguments);
        CHECK_EXCEPTION();
        array = jsCast<JSArray*>(arrayResult);
    }

    RETURN(JSImmutableButterfly::createFromArray(globalObject, vm, array));
}

} // namespace JSC
