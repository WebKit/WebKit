/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BytecodeUseDef.h"

namespace JSC {

#define CALL_FUNCTOR(__arg) \
    functor(__bytecode.m_##__arg);

#define USES_OR_DEFS(__opcode, ...) \
    case __opcode::opcodeID: { \
        auto __bytecode = instruction->as<__opcode>(); \
        WTF_LAZY_FOR_EACH_TERM(CALL_FUNCTOR, __VA_ARGS__) \
        return; \
    }

#define USES USES_OR_DEFS
#define DEFS USES_OR_DEFS

void computeUsesForBytecodeIndexImpl(VirtualRegister scopeRegister, const Instruction* instruction, const Function<void(VirtualRegister)>& functor)
{
    OpcodeID opcodeID = instruction->opcodeID();

    auto handleNewArrayLike = [&](auto op) {
        int base = op.m_argv.offset();
        for (int i = 0; i < static_cast<int>(op.m_argc); i++)
            functor(VirtualRegister { base - i });
    };

    auto handleOpCallLike = [&](auto op) {
        functor(op.m_callee);
        int lastArg = -static_cast<int>(op.m_argv) + CallFrame::thisArgumentOffset();
        for (int i = 0; i < static_cast<int>(op.m_argc); i++)
            functor(VirtualRegister { lastArg + i });
        if (opcodeID == op_call_eval)
            functor(scopeRegister);
        return;
    };

    switch (opcodeID) {
    case op_wide16:
    case op_wide32:
        RELEASE_ASSERT_NOT_REACHED();

    // No uses.
    case op_new_regexp:
    case op_debug:
    case op_jneq_ptr:
    case op_loop_hint:
    case op_jmp:
    case op_new_object:
    case op_new_promise:
    case op_new_generator:
    case op_enter:
    case op_argument_count:
    case op_catch:
    case op_profile_control_flow:
    case op_create_direct_arguments:
    case op_create_cloned_arguments:
    case op_create_arguments_butterfly:
    case op_get_rest_length:
    case op_check_traps:
    case op_get_argument:
    case op_nop:
    case op_unreachable:
    case op_super_sampler_begin:
    case op_super_sampler_end:
        return;

    USES(OpGetScope, dst)
    USES(OpToThis, srcDst)
    USES(OpCheckTdz, targetVirtualRegister)
    USES(OpIdentityWithProfile, srcDst)
    USES(OpProfileType, targetVirtualRegister);
    USES(OpThrow, value)
    USES(OpThrowStaticError, message)
    USES(OpEnd, value)
    USES(OpRet, value)
    USES(OpJtrue, condition)
    USES(OpJfalse, condition)
    USES(OpJeqNull, value)
    USES(OpJneqNull, value)
    USES(OpJundefinedOrNull, value)
    USES(OpJnundefinedOrNull, value)
    USES(OpDec, srcDst)
    USES(OpInc, srcDst)
    USES(OpLogShadowChickenPrologue, scope)

    USES(OpJless, lhs, rhs)
    USES(OpJlesseq, lhs, rhs)
    USES(OpJgreater, lhs, rhs)
    USES(OpJgreatereq, lhs, rhs)
    USES(OpJnless, lhs, rhs)
    USES(OpJnlesseq, lhs, rhs)
    USES(OpJngreater, lhs, rhs)
    USES(OpJngreatereq, lhs, rhs)
    USES(OpJeq, lhs, rhs)
    USES(OpJneq, lhs, rhs)
    USES(OpJstricteq, lhs, rhs)
    USES(OpJnstricteq, lhs, rhs)
    USES(OpJbelow, lhs, rhs)
    USES(OpJbeloweq, lhs, rhs)
    USES(OpSetFunctionName, function, name)
    USES(OpLogShadowChickenTail, thisValue, scope)

    USES(OpPutByVal, base, property, value)
    USES(OpPutByValDirect, base, property, value)

    USES(OpPutById, base, value)
    USES(OpPutToScope, scope, value)
    USES(OpPutToArguments, arguments, value)

    USES(OpPutByIdWithThis, base, thisValue, value)

    USES(OpPutByValWithThis, base, thisValue, property, value)

    USES(OpPutGetterById, base, accessor)
    USES(OpPutSetterById, base, accessor)

    USES(OpPutGetterSetterById, base, getter, setter)

    USES(OpPutGetterByVal, base, property, accessor)
    USES(OpPutSetterByVal, base, property, accessor)

    USES(OpDefineDataProperty, base, property, value, attributes)

    USES(OpDefineAccessorProperty, base, property, getter, setter, attributes)

    USES(OpSpread, argument)
    USES(OpGetPropertyEnumerator, base)
    USES(OpGetEnumerableLength, base)
    USES(OpNewFuncExp, scope)
    USES(OpNewGeneratorFuncExp, scope)
    USES(OpNewAsyncFuncExp, scope)
    USES(OpToIndexString, index)
    USES(OpCreateLexicalEnvironment, scope, symbolTable, initialValue)
    USES(OpCreateGeneratorFrameEnvironment, scope, symbolTable, initialValue)
    USES(OpResolveScope, scope)
    USES(OpResolveScopeForHoistingFuncDeclInEval, scope)
    USES(OpGetFromScope, scope)
    USES(OpToPrimitive, src)
    USES(OpToPropertyKey, src)
    USES(OpTryGetById, base)
    USES(OpGetById, base)
    USES(OpGetByIdDirect, base)
    USES(OpInById, base)
    USES(OpTypeof, value)
    USES(OpIsEmpty, operand)
    USES(OpIsUndefined, operand)
    USES(OpIsUndefinedOrNull, operand)
    USES(OpIsBoolean, operand)
    USES(OpIsNumber, operand)
    USES(OpIsObject, operand)
    USES(OpIsObjectOrNull, operand)
    USES(OpIsCellWithType, operand)
    USES(OpIsFunction, operand)
    USES(OpToNumber, operand)
    USES(OpToNumeric, operand)
    USES(OpToString, operand)
    USES(OpToObject, operand)
    USES(OpNegate, operand)
    USES(OpBitnot, operand)
    USES(OpEqNull, operand)
    USES(OpNeqNull, operand)
    USES(OpNot, operand)
    USES(OpUnsigned, operand)
    USES(OpMov, src)
    USES(OpNewArrayWithSize, length)
    USES(OpCreateThis, callee)
    USES(OpCreatePromise, callee)
    USES(OpCreateGenerator, callee)
    USES(OpCreateAsyncGenerator, callee)
    USES(OpDelById, base)
    USES(OpNewFunc, scope)
    USES(OpNewAsyncGeneratorFunc, scope)
    USES(OpNewAsyncGeneratorFuncExp, scope)
    USES(OpNewGeneratorFunc, scope)
    USES(OpNewAsyncFunc, scope)
    USES(OpGetParentScope, scope)
    USES(OpCreateScopedArguments, scope)
    USES(OpCreateRest, arraySize)
    USES(OpGetFromArguments, arguments)
    USES(OpNewArrayBuffer, immutableButterfly)

    USES(OpHasGenericProperty, base, property)
    USES(OpHasIndexedProperty, base, property)
    USES(OpEnumeratorStructurePname, enumerator, index)
    USES(OpEnumeratorGenericPname, enumerator, index)
    USES(OpGetByVal, base, property)
    USES(OpInByVal, base, property)
    USES(OpOverridesHasInstance, constructor, hasInstanceValue)
    USES(OpInstanceof, value, prototype)
    USES(OpAdd, lhs, rhs)
    USES(OpMul, lhs, rhs)
    USES(OpDiv, lhs, rhs)
    USES(OpMod, lhs, rhs)
    USES(OpSub, lhs, rhs)
    USES(OpPow, lhs, rhs)
    USES(OpLshift, lhs, rhs)
    USES(OpRshift, lhs, rhs)
    USES(OpUrshift, lhs, rhs)
    USES(OpBitand, lhs, rhs)
    USES(OpBitxor, lhs, rhs)
    USES(OpBitor, lhs, rhs)
    USES(OpLess, lhs, rhs)
    USES(OpLesseq, lhs, rhs)
    USES(OpGreater, lhs, rhs)
    USES(OpGreatereq, lhs, rhs)
    USES(OpBelow, lhs, rhs)
    USES(OpBeloweq, lhs, rhs)
    USES(OpNstricteq, lhs, rhs)
    USES(OpStricteq, lhs, rhs)
    USES(OpNeq, lhs, rhs)
    USES(OpEq, lhs, rhs)
    USES(OpPushWithScope, currentScope, newScope)
    USES(OpGetByIdWithThis, base, thisValue)
    USES(OpDelByVal, base, property)
    USES(OpTailCallForwardArguments, callee, thisValue)

    USES(OpGetByValWithThis, base, thisValue, property)
    USES(OpInstanceofCustom, value, constructor, hasInstanceValue)
    USES(OpHasStructureProperty, base, property, enumerator)
    USES(OpConstructVarargs, callee, thisValue, arguments)
    USES(OpCallVarargs, callee, thisValue, arguments)
    USES(OpTailCallVarargs, callee, thisValue, arguments)

    USES(OpGetDirectPname, base, property, index, enumerator)

    USES(OpSwitchString, scrutinee)
    USES(OpSwitchChar, scrutinee)
    USES(OpSwitchImm, scrutinee)

    USES(OpGetInternalField, base)
    USES(OpPutInternalField, base, value)

    USES(OpYield, generator, argument)

    case op_new_array_with_spread:
        handleNewArrayLike(instruction->as<OpNewArrayWithSpread>());
        return;
    case op_new_array:
        handleNewArrayLike(instruction->as<OpNewArray>());
        return;

    case op_strcat: {
        auto bytecode = instruction->as<OpStrcat>();
        int base = bytecode.m_src.offset();
        for (int i = 0; i < bytecode.m_count; i++)
            functor(VirtualRegister { base - i });
        return;
    }

    case op_construct:
        handleOpCallLike(instruction->as<OpConstruct>());
        return;
    case op_call_eval:
        handleOpCallLike(instruction->as<OpCallEval>());
        return;
    case op_call:
        handleOpCallLike(instruction->as<OpCall>());
        return;
    case op_tail_call:
        handleOpCallLike(instruction->as<OpTailCall>());
        return;

    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

void computeDefsForBytecodeIndexImpl(unsigned numVars, const Instruction* instruction, const Function<void(VirtualRegister)>& functor)
{
    switch (instruction->opcodeID()) {
    case op_wide16:
    case op_wide32:
        RELEASE_ASSERT_NOT_REACHED();

    // These don't define anything.
    case op_put_to_scope:
    case op_end:
    case op_throw:
    case op_throw_static_error:
    case op_check_tdz:
    case op_debug:
    case op_ret:
    case op_jmp:
    case op_jtrue:
    case op_jfalse:
    case op_jeq_null:
    case op_jneq_null:
    case op_jundefined_or_null:
    case op_jnundefined_or_null:
    case op_jneq_ptr:
    case op_jless:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_jeq:
    case op_jneq:
    case op_jstricteq:
    case op_jnstricteq:
    case op_jbelow:
    case op_jbeloweq:
    case op_loop_hint:
    case op_switch_imm:
    case op_switch_char:
    case op_switch_string:
    case op_put_by_id:
    case op_put_by_id_with_this:
    case op_put_by_val_with_this:
    case op_put_getter_by_id:
    case op_put_setter_by_id:
    case op_put_getter_setter_by_id:
    case op_put_getter_by_val:
    case op_put_setter_by_val:
    case op_put_by_val:
    case op_put_by_val_direct:
    case op_put_internal_field:
    case op_define_data_property:
    case op_define_accessor_property:
    case op_profile_type:
    case op_profile_control_flow:
    case op_put_to_arguments:
    case op_set_function_name:
    case op_check_traps:
    case op_log_shadow_chicken_prologue:
    case op_log_shadow_chicken_tail:
    case op_yield:
    case op_nop:
    case op_unreachable:
    case op_super_sampler_begin:
    case op_super_sampler_end:
#define LLINT_HELPER_OPCODES(opcode, length) case opcode:
        FOR_EACH_LLINT_OPCODE_EXTENSION(LLINT_HELPER_OPCODES);
#undef LLINT_HELPER_OPCODES
        return;
    // These all have a single destination for the first argument.
    DEFS(OpArgumentCount, dst)
    DEFS(OpToIndexString, dst)
    DEFS(OpGetEnumerableLength, dst)
    DEFS(OpHasIndexedProperty, dst)
    DEFS(OpHasStructureProperty, dst)
    DEFS(OpHasGenericProperty, dst)
    DEFS(OpGetDirectPname, dst)
    DEFS(OpGetPropertyEnumerator, dst)
    DEFS(OpEnumeratorStructurePname, dst)
    DEFS(OpEnumeratorGenericPname, dst)
    DEFS(OpGetParentScope, dst)
    DEFS(OpPushWithScope, dst)
    DEFS(OpCreateLexicalEnvironment, dst)
    DEFS(OpCreateGeneratorFrameEnvironment, dst)
    DEFS(OpResolveScope, dst)
    DEFS(OpResolveScopeForHoistingFuncDeclInEval, dst)
    DEFS(OpStrcat, dst)
    DEFS(OpToPrimitive, dst)
    DEFS(OpToPropertyKey, dst)
    DEFS(OpCreateThis, dst)
    DEFS(OpCreatePromise, dst)
    DEFS(OpCreateGenerator, dst)
    DEFS(OpCreateAsyncGenerator, dst)
    DEFS(OpNewArray, dst)
    DEFS(OpNewArrayWithSpread, dst)
    DEFS(OpSpread, dst)
    DEFS(OpNewArrayBuffer, dst)
    DEFS(OpNewArrayWithSize, dst)
    DEFS(OpNewRegexp, dst)
    DEFS(OpNewFunc, dst)
    DEFS(OpNewFuncExp, dst)
    DEFS(OpNewGeneratorFunc, dst)
    DEFS(OpNewGeneratorFuncExp, dst)
    DEFS(OpNewAsyncGeneratorFunc, dst)
    DEFS(OpNewAsyncGeneratorFuncExp, dst)
    DEFS(OpNewAsyncFunc, dst)
    DEFS(OpNewAsyncFuncExp, dst)
    DEFS(OpCallVarargs, dst)
    DEFS(OpTailCallVarargs, dst)
    DEFS(OpTailCallForwardArguments, dst)
    DEFS(OpConstructVarargs, dst)
    DEFS(OpGetFromScope, dst)
    DEFS(OpCall, dst)
    DEFS(OpTailCall, dst)
    DEFS(OpCallEval, dst)
    DEFS(OpConstruct, dst)
    DEFS(OpTryGetById, dst)
    DEFS(OpGetById, dst)
    DEFS(OpGetByIdDirect, dst)
    DEFS(OpGetByIdWithThis, dst)
    DEFS(OpGetByValWithThis, dst)
    DEFS(OpOverridesHasInstance, dst)
    DEFS(OpInstanceof, dst)
    DEFS(OpInstanceofCustom, dst)
    DEFS(OpGetByVal, dst)
    DEFS(OpTypeof, dst)
    DEFS(OpIdentityWithProfile, srcDst)
    DEFS(OpIsEmpty, dst)
    DEFS(OpIsUndefined, dst)
    USES(OpIsUndefinedOrNull, dst)
    DEFS(OpIsBoolean, dst)
    DEFS(OpIsNumber, dst)
    DEFS(OpIsObject, dst)
    DEFS(OpIsObjectOrNull, dst)
    DEFS(OpIsCellWithType, dst)
    DEFS(OpIsFunction, dst)
    DEFS(OpInById, dst)
    DEFS(OpInByVal, dst)
    DEFS(OpToNumber, dst)
    DEFS(OpToNumeric, dst)
    DEFS(OpToString, dst)
    DEFS(OpToObject, dst)
    DEFS(OpNegate, dst)
    DEFS(OpAdd, dst)
    DEFS(OpMul, dst)
    DEFS(OpDiv, dst)
    DEFS(OpMod, dst)
    DEFS(OpSub, dst)
    DEFS(OpPow, dst)
    DEFS(OpLshift, dst)
    DEFS(OpRshift, dst)
    DEFS(OpUrshift, dst)
    DEFS(OpBitand, dst)
    DEFS(OpBitxor, dst)
    DEFS(OpBitor, dst)
    DEFS(OpBitnot, dst)
    DEFS(OpInc, srcDst)
    DEFS(OpDec, srcDst)
    DEFS(OpEq, dst)
    DEFS(OpNeq, dst)
    DEFS(OpStricteq, dst)
    DEFS(OpNstricteq, dst)
    DEFS(OpLess, dst)
    DEFS(OpLesseq, dst)
    DEFS(OpGreater, dst)
    DEFS(OpGreatereq, dst)
    DEFS(OpBelow, dst)
    DEFS(OpBeloweq, dst)
    DEFS(OpNeqNull, dst)
    DEFS(OpEqNull, dst)
    DEFS(OpNot, dst)
    DEFS(OpMov, dst)
    DEFS(OpNewObject, dst)
    DEFS(OpNewPromise, dst)
    DEFS(OpNewGenerator, dst)
    DEFS(OpToThis, srcDst)
    DEFS(OpGetScope, dst)
    DEFS(OpCreateDirectArguments, dst)
    DEFS(OpCreateScopedArguments, dst)
    DEFS(OpCreateClonedArguments, dst)
    DEFS(OpCreateArgumentsButterfly, dst)
    DEFS(OpDelById, dst)
    DEFS(OpDelByVal, dst)
    DEFS(OpUnsigned, dst)
    DEFS(OpGetFromArguments, dst)
    DEFS(OpGetArgument, dst)
    DEFS(OpCreateRest, dst)
    DEFS(OpGetRestLength, dst)
    DEFS(OpGetInternalField, dst)

    DEFS(OpCatch, exception, thrownValue)

    case op_enter: {
        for (unsigned i = numVars; i--;)
            functor(virtualRegisterForLocal(i));
        return;
    }
    }
}

#undef CALL_FUNCTOR
#undef USES_OR_DEFS
#undef USES
#undef DEFS
} // namespace JSC
