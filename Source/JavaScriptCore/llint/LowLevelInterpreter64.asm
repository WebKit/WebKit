# Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.


# Some value representation constants.
const TagBitTypeOther = 0x2
const TagBitBool      = 0x4
const TagBitUndefined = 0x8
const ValueEmpty      = 0x0
const ValueFalse      = TagBitTypeOther | TagBitBool
const ValueTrue       = TagBitTypeOther | TagBitBool | 1
const ValueUndefined  = TagBitTypeOther | TagBitUndefined
const ValueNull       = TagBitTypeOther

# Utilities.
macro dispatch(advance)
    addp advance, PC
    jmp [PB, PC, 8]
end

macro dispatchInt(advance)
    addi advance, PC
    jmp [PB, PC, 8]
end

macro dispatchAfterCall()
    loadi ArgumentCount + TagOffset[cfr], PC
    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructions[PB], PB
    jmp [PB, PC, 8]
end

macro cCall2(function, arg1, arg2)
    move arg1, t5
    move arg2, t4
    call function
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function, arg1, arg2, arg3, arg4)
    move arg1, t5
    move arg2, t4
    move arg3, t1
    move arg4, t2
    call function
end

macro prepareStateForCCall()
    leap [PB, PC, 8], PC
    move PB, t3
end

macro restoreStateAfterCCall()
    move t0, PC
    move t1, cfr
    move t3, PB
    subp PB, PC
    urshiftp 3, PC
end

macro callSlowPath(slowPath)
    prepareStateForCCall()
    cCall2(slowPath, cfr, PC)
    restoreStateAfterCCall()
end

macro traceOperand(fromWhere, operand)
    prepareStateForCCall()
    cCall4(_llint_trace_operand, cfr, PC, fromWhere, operand)
    restoreStateAfterCCall()
end

macro traceValue(fromWhere, operand)
    prepareStateForCCall()
    cCall4(_llint_trace_value, cfr, PC, fromWhere, operand)
    restoreStateAfterCCall()
end

# Call a slow path for call call opcodes.
macro callCallSlowPath(advance, slowPath, action)
    addi advance, PC, t0
    storei t0, ArgumentCount + TagOffset[cfr]
    prepareStateForCCall()
    cCall2(slowPath, cfr, PC)
    move t1, cfr
    action(t0)
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro()
            storei PC, ArgumentCount + TagOffset[cfr]
            prepareStateForCCall()
            cCall2(_llint_loop_osr, cfr, PC)
            move t1, cfr
            btpz t0, .recover
            jmp t0
        .recover:
            move t3, PB
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

# Index and value must be different registers. Index may be clobbered.
macro loadConstantOrVariable(index, value)
    bpgteq index, FirstConstantRegisterIndex, .constant
    loadp [cfr, index, 8], value
    jmp .done
.constant:
    loadp CodeBlock[cfr], value
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[value], value
    subp FirstConstantRegisterIndex, index
    loadp [value, index, 8], value
.done:
end

macro loadConstantOrVariableInt32(index, value, slow)
    loadConstantOrVariable(index, value)
    bpb value, tagTypeNumber, slow
end

macro loadConstantOrVariableCell(index, value, slow)
    loadConstantOrVariable(index, value)
    btpnz value, tagMask, slow
end

macro writeBarrier(value)
    # Nothing to do, since we don't have a generational or incremental collector.
end

macro valueProfile(value, profile)
    if VALUE_PROFILER
        storep value, ValueProfile::m_buckets[profile]
    end
end


# Entrypoints into the interpreter.

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slow_path)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    prepareStateForCCall()
    cCall2(slow_path, cfr, PC)   # This slow_path has a simple protocol: t0 = 0 => no error, t0 != 0 => error
    move t1, cfr
    btiz t0, .continue
    loadp JITStackFrame::globalData[sp], t1
    loadp JSGlobalData::callFrameForThrow[t1], t0
    jmp JSGlobalData::targetMachinePCForThrow[t1]
.continue:
    # Reload CodeBlock and reset PC, since the slow_path clobbered them.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructions[t1], PB
    move 0, PC
    jmp doneLabel
end


# Instruction implementations

_llint_op_enter:
    traceExecution()
    loadp CodeBlock[cfr], t2
    loadi CodeBlock::m_numVars[t2], t2
    btiz t2, .opEnterDone
    move ValueUndefined, t0
.opEnterLoop:
    subi 1, t2
    storep t0, [cfr, t2, 8]
    btinz t2, .opEnterLoop
.opEnterDone:
    dispatch(1)


_llint_op_create_activation:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    bpneq [cfr, t0, 8], ValueEmpty, .opCreateActivationDone
    callSlowPath(_llint_slow_path_create_activation)
.opCreateActivationDone:
    dispatch(2)


_llint_op_init_lazy_reg:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    storep ValueEmpty, [cfr, t0, 8]
    dispatch(2)


_llint_op_create_arguments:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    bpneq [cfr, t0, 8], ValueEmpty, .opCreateArgumentsDone
    callSlowPath(_llint_slow_path_create_arguments)
.opCreateArgumentsDone:
    dispatch(2)


_llint_op_create_this:
    traceExecution()
    loadp Callee[cfr], t0
    loadp JSFunction::m_cachedInheritorID[t0], t2
    btpz t2, .opCreateThisSlow
    allocateBasicJSObject(JSFinalObjectSizeClassIndex, JSGlobalData::jsFinalObjectClassInfo, t2, t0, t1, t3, .opCreateThisSlow)
    loadis 8[PB, PC, 8], t1
    storep t0, [cfr, t1, 8]
    dispatch(2)

.opCreateThisSlow:
    callSlowPath(_llint_slow_path_create_this)
    dispatch(2)


_llint_op_get_callee:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadp Callee[cfr], t1
    storep t1, [cfr, t0, 8]
    dispatch(2)


_llint_op_convert_this:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadp [cfr, t0, 8], t0
    btpnz t0, tagMask, .opConvertThisSlow
    loadp JSCell::m_structure[t0], t0
    bbb Structure::m_typeInfo + TypeInfo::m_type[t0], ObjectType, .opConvertThisSlow
    dispatch(2)

.opConvertThisSlow:
    callSlowPath(_llint_slow_path_convert_this)
    dispatch(2)


_llint_op_new_object:
    traceExecution()
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_emptyObjectStructure[t0], t1
    allocateBasicJSObject(JSFinalObjectSizeClassIndex, JSGlobalData::jsFinalObjectClassInfo, t1, t0, t2, t3, .opNewObjectSlow)
    loadis 8[PB, PC, 8], t1
    storep t0, [cfr, t1, 8]
    dispatch(2)

.opNewObjectSlow:
    callSlowPath(_llint_slow_path_new_object)
    dispatch(2)


_llint_op_mov:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t0
    loadConstantOrVariable(t1, t2)
    storep t2, [cfr, t0, 8]
    dispatch(3)


_llint_op_not:
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t1
    loadConstantOrVariable(t0, t2)
    xorp ValueFalse, t2
    btpnz t2, ~1, .opNotSlow
    xorp ValueTrue, t2
    storep t2, [cfr, t1, 8]
    dispatch(3)

.opNotSlow:
    callSlowPath(_llint_slow_path_not)
    dispatch(3)


macro equalityComparison(integerComparison, slowPath)
    traceExecution()
    loadis 24[PB, PC, 8], t0
    loadis 16[PB, PC, 8], t2
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariableInt32(t0, t1, .slow)
    loadConstantOrVariableInt32(t2, t0, .slow)
    integerComparison(t0, t1, t0)
    orp ValueFalse, t0
    storep t0, [cfr, t3, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_eq:
    equalityComparison(
        macro (left, right, result) cieq left, right, result end,
        _llint_slow_path_eq)


_llint_op_neq:
    equalityComparison(
        macro (left, right, result) cineq left, right, result end,
        _llint_slow_path_neq)


macro equalNullComparison()
    loadis 16[PB, PC, 8], t0
    loadp [cfr, t0, 8], t0
    btpnz t0, tagMask, .immediate
    loadp JSCell::m_structure[t0], t2
    tbnz Structure::m_typeInfo + TypeInfo::m_flags[t2], MasqueradesAsUndefined, t0
    jmp .done
.immediate:
    andp ~TagBitUndefined, t0
    cpeq t0, ValueNull, t0
.done:
end

_llint_op_eq_null:
    traceExecution()
    equalNullComparison()
    loadis 8[PB, PC, 8], t1
    orp ValueFalse, t0
    storep t0, [cfr, t1, 8]
    dispatch(3)


_llint_op_neq_null:
    traceExecution()
    equalNullComparison()
    loadis 8[PB, PC, 8], t1
    xorp ValueTrue, t0
    storep t0, [cfr, t1, 8]
    dispatch(3)


macro strictEq(equalityOperation, slowPath)
    traceExecution()
    loadis 24[PB, PC, 8], t0
    loadis 16[PB, PC, 8], t2
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
    move t0, t2
    orp t1, t2
    btpz t2, tagMask, .slow
    bpaeq t0, tagTypeNumber, .leftOK
    btpnz t0, tagTypeNumber, .slow
.leftOK:
    bpaeq t1, tagTypeNumber, .rightOK
    btpnz t1, tagTypeNumber, .slow
.rightOK:
    equalityOperation(t0, t1, t0)
    loadis 8[PB, PC, 8], t1
    orp ValueFalse, t0
    storep t0, [cfr, t1, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_stricteq:
    strictEq(
        macro (left, right, result) cpeq left, right, result end,
        _llint_slow_path_stricteq)


_llint_op_nstricteq:
    strictEq(
        macro (left, right, result) cpneq left, right, result end,
        _llint_slow_path_nstricteq)


macro preOp(arithmeticOperation, slowPath)
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadp [cfr, t0, 8], t1
    bpb t1, tagTypeNumber, .slow
    arithmeticOperation(t1, .slow)
    orp tagTypeNumber, t1
    storep t1, [cfr, t0, 8]
    dispatch(2)

.slow:
    callSlowPath(slowPath)
    dispatch(2)
end

_llint_op_pre_inc:
    preOp(
        macro (value, slow) baddio 1, value, slow end,
        _llint_slow_path_pre_inc)


_llint_op_pre_dec:
    preOp(
        macro (value, slow) bsubio 1, value, slow end,
        _llint_slow_path_pre_dec)


macro postOp(arithmeticOperation, slowPath)
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t1
    loadp [cfr, t0, 8], t2
    bieq t0, t1, .done
    bpb t2, tagTypeNumber, .slow
    move t2, t3
    arithmeticOperation(t3, .slow)
    orp tagTypeNumber, t3
    storep t2, [cfr, t1, 8]
    storep t3, [cfr, t0, 8]
.done:
    dispatch(3)

.slow:
    callSlowPath(slowPath)
    dispatch(3)
end

_llint_op_post_inc:
    postOp(
        macro (value, slow) baddio 1, value, slow end,
        _llint_slow_path_post_inc)


_llint_op_post_dec:
    postOp(
        macro (value, slow) bsubio 1, value, slow end,
        _llint_slow_path_post_dec)


_llint_op_to_jsnumber:
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t1
    loadConstantOrVariable(t0, t2)
    bpaeq t2, tagTypeNumber, .opToJsnumberIsImmediate
    btpz t2, tagTypeNumber, .opToJsnumberSlow
.opToJsnumberIsImmediate:
    storep t2, [cfr, t1, 8]
    dispatch(3)

.opToJsnumberSlow:
    callSlowPath(_llint_slow_path_to_jsnumber)
    dispatch(3)


_llint_op_negate:
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t1
    loadConstantOrVariable(t0, t2)
    bpb t2, tagTypeNumber, .opNegateNotInt
    btiz t2, 0x7fffffff, .opNegateSlow
    negi t2
    orp tagTypeNumber, t2
    storep t2, [cfr, t1, 8]
    dispatch(3)
.opNegateNotInt:
    btpz t2, tagTypeNumber, .opNegateSlow
    xorp 0x8000000000000000, t2
    storep t2, [cfr, t1, 8]
    dispatch(3)

.opNegateSlow:
    callSlowPath(_llint_slow_path_negate)
    dispatch(3)


macro binaryOpCustomStore(integerOperationAndStore, doubleOperation, slowPath)
    loadis 24[PB, PC, 8], t0
    loadis 16[PB, PC, 8], t2
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
    bpb t0, tagTypeNumber, .op1NotInt
    bpb t1, tagTypeNumber, .op2NotInt
    loadis 8[PB, PC, 8], t2
    integerOperationAndStore(t1, t0, .slow, t2)
    dispatch(5)

.op1NotInt:
    # First operand is definitely not an int, the second operand could be anything.
    btpz t0, tagTypeNumber, .slow
    bpaeq t1, tagTypeNumber, .op1NotIntOp2Int
    btpz t1, tagTypeNumber, .slow
    addp tagTypeNumber, t1
    fp2d t1, ft1
    jmp .op1NotIntReady
.op1NotIntOp2Int:
    ci2d t1, ft1
.op1NotIntReady:
    loadis 8[PB, PC, 8], t2
    addp tagTypeNumber, t0
    fp2d t0, ft0
    doubleOperation(ft1, ft0)
    fd2p ft0, t0
    subp tagTypeNumber, t0
    storep t0, [cfr, t2, 8]
    dispatch(5)

.op2NotInt:
    # First operand is definitely an int, the second is definitely not.
    loadis 8[PB, PC, 8], t2
    btpz t1, tagTypeNumber, .slow
    ci2d t0, ft0
    addp tagTypeNumber, t1
    fp2d t1, ft1
    doubleOperation(ft1, ft0)
    fd2p ft0, t0
    subp tagTypeNumber, t0
    storep t0, [cfr, t2, 8]
    dispatch(5)

.slow:
    callSlowPath(slowPath)
    dispatch(5)
end

macro binaryOp(integerOperation, doubleOperation, slowPath)
    binaryOpCustomStore(
        macro (left, right, slow, index)
            integerOperation(left, right, slow)
            orp tagTypeNumber, right
            storep right, [cfr, index, 8]
        end,
        doubleOperation, slowPath)
end

_llint_op_add:
    traceExecution()
    binaryOp(
        macro (left, right, slow) baddio left, right, slow end,
        macro (left, right) addd left, right end,
        _llint_slow_path_add)


_llint_op_mul:
    traceExecution()
    binaryOpCustomStore(
        macro (left, right, slow, index)
            # Assume t3 is scratchable.
            move right, t3
            bmulio left, t3, slow
            btinz t3, .done
            bilt left, 0, slow
            bilt right, 0, slow
        .done:
            orp tagTypeNumber, t3
            storep t3, [cfr, index, 8]
        end,
        macro (left, right) muld left, right end,
        _llint_slow_path_mul)


_llint_op_sub:
    traceExecution()
    binaryOp(
        macro (left, right, slow) bsubio left, right, slow end,
        macro (left, right) subd left, right end,
        _llint_slow_path_sub)


_llint_op_div:
    traceExecution()
    binaryOpCustomStore(
        macro (left, right, slow, index)
            # Assume t3 is scratchable.
            btiz left, slow
            bineq left, -1, .notNeg2TwoThe31DivByNeg1
            bieq right, -2147483648, .slow
        .notNeg2TwoThe31DivByNeg1:
            btinz right, .intOK
            bilt left, 0, slow
        .intOK:
            move left, t3
            move right, t0
            cdqi
            idivi t3
            btinz t1, slow
            orp tagTypeNumber, t0
            storep t0, [cfr, index, 8]
        end,
        macro (left, right) divd left, right end,
        _llint_slow_path_div)


macro bitOp(operation, slowPath, advance)
    loadis 24[PB, PC, 8], t0
    loadis 16[PB, PC, 8], t2
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariable(t0, t1)
    loadConstantOrVariable(t2, t0)
    bpb t0, tagTypeNumber, .slow
    bpb t1, tagTypeNumber, .slow
    operation(t1, t0, .slow)
    orp tagTypeNumber, t0
    storep t0, [cfr, t3, 8]
    dispatch(advance)

.slow:
    callSlowPath(slowPath)
    dispatch(advance)
end

_llint_op_lshift:
    traceExecution()
    bitOp(
        macro (left, right, slow) lshifti left, right end,
        _llint_slow_path_lshift,
        4)


_llint_op_rshift:
    traceExecution()
    bitOp(
        macro (left, right, slow) rshifti left, right end,
        _llint_slow_path_rshift,
        4)


_llint_op_urshift:
    traceExecution()
    bitOp(
        macro (left, right, slow)
            urshifti left, right
            bilt right, 0, slow
        end,
        _llint_slow_path_urshift,
        4)


_llint_op_bitand:
    traceExecution()
    bitOp(
        macro (left, right, slow) andi left, right end,
        _llint_slow_path_bitand,
        5)


_llint_op_bitxor:
    traceExecution()
    bitOp(
        macro (left, right, slow) xori left, right end,
        _llint_slow_path_bitxor,
        5)


_llint_op_bitor:
    traceExecution()
    bitOp(
        macro (left, right, slow) ori left, right end,
        _llint_slow_path_bitor,
        5)


_llint_op_check_has_instance:
    traceExecution()
    loadis 8[PB, PC, 8], t1
    loadConstantOrVariableCell(t1, t0, .opCheckHasInstanceSlow)
    loadp JSCell::m_structure[t0], t0
    btbz Structure::m_typeInfo + TypeInfo::m_flags[t0], ImplementsHasInstance, .opCheckHasInstanceSlow
    dispatch(2)

.opCheckHasInstanceSlow:
    callSlowPath(_llint_slow_path_check_has_instance)
    dispatch(2)


_llint_op_instanceof:
    traceExecution()
    # Check that baseVal implements the default HasInstance behavior.
    # FIXME: This should be deprecated.
    loadis 24[PB, PC, 8], t1
    loadConstantOrVariable(t1, t0)
    loadp JSCell::m_structure[t0], t0
    btbz Structure::m_typeInfo + TypeInfo::m_flags[t0], ImplementsDefaultHasInstance, .opInstanceofSlow
    
    # Actually do the work.
    loadis 32[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariableCell(t0, t1, .opInstanceofSlow)
    loadp JSCell::m_structure[t1], t2
    bbb Structure::m_typeInfo + TypeInfo::m_type[t2], ObjectType, .opInstanceofSlow
    loadis 16[PB, PC, 8], t0
    loadConstantOrVariableCell(t0, t2, .opInstanceofSlow)
    
    # Register state: t1 = prototype, t2 = value
    move 1, t0
.opInstanceofLoop:
    loadp JSCell::m_structure[t2], t2
    loadp Structure::m_prototype[t2], t2
    bpeq t2, t1, .opInstanceofDone
    btpz t2, tagMask, .opInstanceofLoop

    move 0, t0
.opInstanceofDone:
    orp ValueFalse, t0
    storep t0, [cfr, t3, 8]
    dispatch(5)

.opInstanceofSlow:
    callSlowPath(_llint_slow_path_instanceof)
    dispatch(5)


_llint_op_is_undefined:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t1, t0)
    btpz t0, tagMask, .opIsUndefinedCell
    cpeq t0, ValueUndefined, t3
    orp ValueFalse, t3
    storep t3, [cfr, t2, 8]
    dispatch(3)
.opIsUndefinedCell:
    loadp JSCell::m_structure[t0], t0
    tbnz Structure::m_typeInfo + TypeInfo::m_flags[t0], MasqueradesAsUndefined, t1
    orp ValueFalse, t1
    storep t1, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_boolean:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t1, t0)
    xorp ValueFalse, t0
    tpz t0, ~1, t0
    orp ValueFalse, t0
    storep t0, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_number:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t1, t0)
    tpnz t0, tagTypeNumber, t1
    orp ValueFalse, t1
    storep t1, [cfr, t2, 8]
    dispatch(3)


_llint_op_is_string:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t1, t0)
    btpnz t0, tagMask, .opIsStringNotCell
    loadp JSCell::m_structure[t0], t0
    cbeq Structure::m_typeInfo + TypeInfo::m_type[t0], StringType, t1
    orp ValueFalse, t1
    storep t1, [cfr, t2, 8]
    dispatch(3)
.opIsStringNotCell:
    storep ValueFalse, [cfr, t2, 8]
    dispatch(3)


macro resolveGlobal(size, slow)
    # Operands are as follows:
    # 8[PB, PC, 8]   Destination for the load.
    # 16[PB, PC, 8]  Property identifier index in the code block.
    # 24[PB, PC, 8]  Structure pointer, initialized to 0 by bytecode generator.
    # 32[PB, PC, 8]  Offset in global object, initialized to 0 by bytecode generator.
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSCell::m_structure[t0], t1
    bpneq t1, 24[PB, PC, 8], slow
    loadis 32[PB, PC, 8], t1
    loadp JSObject::m_propertyStorage[t0], t0
    loadp [t0, t1, 8], t2
    loadis 8[PB, PC, 8], t0
    storep t2, [cfr, t0, 8]
    loadp (size - 1) * 8[PB, PC, 8], t0
    valueProfile(t2, t0)
end

_llint_op_resolve_global:
    traceExecution()
    resolveGlobal(6, .opResolveGlobalSlow)
    dispatch(6)

.opResolveGlobalSlow:
    callSlowPath(_llint_slow_path_resolve_global)
    dispatch(6)


# Gives you the scope in t0, while allowing you to optionally perform additional checks on the
# scopes as they are traversed. scopeCheck() is called with two arguments: the register
# holding the scope, and a register that can be used for scratch. Note that this does not
# use t3, so you can hold stuff in t3 if need be.
macro getScope(deBruijinIndexOperand, scopeCheck)
    loadp ScopeChain[cfr], t0
    loadis deBruijinIndexOperand, t2
    
    btiz t2, .done
    
    loadp CodeBlock[cfr], t1
    bineq CodeBlock::m_codeType[t1], FunctionCode, .loop
    btbz CodeBlock::m_needsFullScopeChain[t1], .loop
    
    loadis CodeBlock::m_activationRegister[t1], t1

    # Need to conditionally skip over one scope.
    btpz [cfr, t1, 8], .noActivation
    scopeCheck(t0, t1)
    loadp ScopeChainNode::next[t0], t0
.noActivation:
    subi 1, t2
    
    btiz t2, .done
.loop:
    scopeCheck(t0, t1)
    loadp ScopeChainNode::next[t0], t0
    subi 1, t2
    btinz t2, .loop

.done:
end

_llint_op_resolve_global_dynamic:
    traceExecution()
    loadp JITStackFrame::globalData[sp], t3
    loadp JSGlobalData::activationStructure[t3], t3
    getScope(
        40[PB, PC, 8],
        macro (scope, scratch)
            loadp ScopeChainNode::object[scope], scratch
            bpneq JSCell::m_structure[scratch], t3, .opResolveGlobalDynamicSuperSlow
        end)
    resolveGlobal(7, .opResolveGlobalDynamicSlow)
    dispatch(7)

.opResolveGlobalDynamicSuperSlow:
    callSlowPath(_llint_slow_path_resolve_for_resolve_global_dynamic)
    dispatch(7)

.opResolveGlobalDynamicSlow:
    callSlowPath(_llint_slow_path_resolve_global_dynamic)
    dispatch(7)


_llint_op_get_scoped_var:
    traceExecution()
    # Operands are as follows:
    # 8[PB, PC, 8]   Destination for the load
    # 16[PB, PC, 8]  Index of register in the scope
    # 24[PB, PC, 8]  De Bruijin index.
    getScope(24[PB, PC, 8], macro (scope, scratch) end)
    loadis 8[PB, PC, 8], t1
    loadis 16[PB, PC, 8], t2
    loadp ScopeChainNode::object[t0], t0
    loadp JSVariableObject::m_registers[t0], t0
    loadp [t0, t2, 8], t3
    storep t3, [cfr, t1, 8]
    loadp 32[PB, PC, 8], t1
    valueProfile(t3, t1)
    dispatch(5)


_llint_op_put_scoped_var:
    traceExecution()
    getScope(16[PB, PC, 8], macro (scope, scratch) end)
    loadis 24[PB, PC, 8], t1
    loadConstantOrVariable(t1, t3)
    loadis 8[PB, PC, 8], t1
    writeBarrier(t3)
    loadp ScopeChainNode::object[t0], t0
    loadp JSVariableObject::m_registers[t0], t0
    storep t3, [t0, t1, 8]
    dispatch(4)


_llint_op_get_global_var:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadis 8[PB, PC, 8], t3
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_registers[t0], t0
    loadp [t0, t1, 8], t2
    storep t2, [cfr, t3, 8]
    loadp 24[PB, PC, 8], t3
    valueProfile(t2, t3)
    dispatch(4)


_llint_op_put_global_var:
    traceExecution()
    loadis 16[PB, PC, 8], t1
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_registers[t0], t0
    loadConstantOrVariable(t1, t2)
    loadis 8[PB, PC, 8], t1
    writeBarrier(t2)
    storep t2, [t0, t1, 8]
    dispatch(3)


_llint_op_get_by_id:
    traceExecution()
    # We only do monomorphic get_by_id caching for now, and we do not modify the
    # opcode. We do, however, allow for the cache to change anytime if fails, since
    # ping-ponging is free. At best we get lucky and the get_by_id will continue
    # to take fast path on the new cache. At worst we take slow path, which is what
    # we would have been doing anyway.
    loadis 16[PB, PC, 8], t0
    loadp 32[PB, PC, 8], t1
    loadConstantOrVariableCell(t0, t3, .opGetByIdSlow)
    loadis 40[PB, PC, 8], t2
    loadp JSObject::m_propertyStorage[t3], t0
    bpneq JSCell::m_structure[t3], t1, .opGetByIdSlow
    loadis 8[PB, PC, 8], t1
    loadp [t0, t2], t3
    storep t3, [cfr, t1, 8]
    loadp 64[PB, PC, 8], t1
    valueProfile(t3, t1)
    dispatch(9)

.opGetByIdSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch(9)


_llint_op_get_arguments_length:
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 8[PB, PC, 8], t1
    btpnz [cfr, t0, 8], .opGetArgumentsLengthSlow
    loadi ArgumentCount + PayloadOffset[cfr], t2
    subi 1, t2
    orp tagTypeNumber, t2
    storep t2, [cfr, t1, 8]
    dispatch(4)

.opGetArgumentsLengthSlow:
    callSlowPath(_llint_slow_path_get_arguments_length)
    dispatch(4)


_llint_op_put_by_id:
    traceExecution()
    loadis 8[PB, PC, 8], t3
    loadp 32[PB, PC, 8], t1
    loadConstantOrVariableCell(t3, t0, .opPutByIdSlow)
    loadis 24[PB, PC, 8], t2
    loadp JSObject::m_propertyStorage[t0], t3
    bpneq JSCell::m_structure[t0], t1, .opPutByIdSlow
    loadis 40[PB, PC, 8], t1
    loadConstantOrVariable(t2, t0)
    writeBarrier(t0)
    storep t0, [t3, t1]
    dispatch(9)

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch(9)


macro putByIdTransition(additionalChecks)
    traceExecution()
    loadis 8[PB, PC, 8], t3
    loadp 32[PB, PC, 8], t1
    loadConstantOrVariableCell(t3, t0, .opPutByIdSlow)
    loadis 24[PB, PC, 8], t2
    bpneq JSCell::m_structure[t0], t1, .opPutByIdSlow
    additionalChecks(t1, t3, .opPutByIdSlow)
    loadis 40[PB, PC, 8], t1
    loadp JSObject::m_propertyStorage[t0], t3
    addp t1, t3
    loadConstantOrVariable(t2, t1)
    writeBarrier(t1)
    storep t1, [t3]
    loadp 48[PB, PC, 8], t1
    storep t1, JSCell::m_structure[t0]
    dispatch(9)
end

_llint_op_put_by_id_transition_direct:
    putByIdTransition(macro (oldStructure, scratch, slow) end)


_llint_op_put_by_id_transition_normal:
    putByIdTransition(
        macro (oldStructure, scratch, slow)
            const protoCell = oldStructure    # Reusing the oldStructure register for the proto
            loadp 56[PB, PC, 8], scratch
            assert(macro (ok) btpnz scratch, ok end)
            loadp StructureChain::m_vector[scratch], scratch
            assert(macro (ok) btpnz scratch, ok end)
            bpeq Structure::m_prototype[oldStructure], ValueNull, .done
        .loop:
            loadp Structure::m_prototype[oldStructure], protoCell
            loadp JSCell::m_structure[protoCell], oldStructure
            bpneq oldStructure, [scratch], slow
            addp 8, scratch
            bpneq Structure::m_prototype[oldStructure], ValueNull, .loop
        .done:
        end)


_llint_op_get_by_val:
    traceExecution()
    loadp CodeBlock[cfr], t1
    loadis 16[PB, PC, 8], t2
    loadis 24[PB, PC, 8], t3
    loadp CodeBlock::m_globalData[t1], t1
    loadConstantOrVariableCell(t2, t0, .opGetByValSlow)
    loadp JSGlobalData::jsArrayClassInfo[t1], t2
    loadConstantOrVariableInt32(t3, t1, .opGetByValSlow)
    sxi2p t1, t1
    bpneq [t0], t2, .opGetByValSlow
    loadp JSArray::m_storage[t0], t3
    biaeq t1, JSArray::m_vectorLength[t0], .opGetByValSlow
    loadis 8[PB, PC, 8], t0
    loadp ArrayStorage::m_vector[t3, t1, 8], t2
    btpz t2, .opGetByValSlow
    storep t2, [cfr, t0, 8]
    loadp 32[PB, PC, 8], t0
    valueProfile(t2, t0)
    dispatch(5)

.opGetByValSlow:
    callSlowPath(_llint_slow_path_get_by_val)
    dispatch(5)


_llint_op_get_argument_by_val:
    traceExecution()
    loadis 16[PB, PC, 8], t0
    loadis 24[PB, PC, 8], t1
    btpnz [cfr, t0, 8], .opGetArgumentByValSlow
    loadConstantOrVariableInt32(t1, t2, .opGetArgumentByValSlow)
    addi 1, t2
    loadi ArgumentCount + PayloadOffset[cfr], t1
    biaeq t2, t1, .opGetArgumentByValSlow
    negi t2
    sxi2p t2, t2
    loadis 8[PB, PC, 8], t3
    loadp ThisArgumentOffset[cfr, t2, 8], t0
    storep t0, [cfr, t3, 8]
    dispatch(5)

.opGetArgumentByValSlow:
    callSlowPath(_llint_slow_path_get_argument_by_val)
    dispatch(5)


_llint_op_get_by_pname:
    traceExecution()
    loadis 24[PB, PC, 8], t1
    loadConstantOrVariable(t1, t0)
    loadis 32[PB, PC, 8], t1
    assertNotConstant(t1)
    bpneq t0, [cfr, t1, 8], .opGetByPnameSlow
    loadis 16[PB, PC, 8], t2
    loadis 40[PB, PC, 8], t3
    loadConstantOrVariableCell(t2, t0, .opGetByPnameSlow)
    assertNotConstant(t3)
    loadp [cfr, t3, 8], t1
    loadp JSCell::m_structure[t0], t2
    bpneq t2, JSPropertyNameIterator::m_cachedStructure[t1], .opGetByPnameSlow
    loadis 48[PB, PC, 8], t3
    loadi PayloadOffset[cfr, t3, 8], t3
    subi 1, t3
    biaeq t3, JSPropertyNameIterator::m_numCacheableSlots[t1], .opGetByPnameSlow
    loadp JSObject::m_propertyStorage[t0], t0
    loadp [t0, t3, 8], t0
    loadis 8[PB, PC, 8], t1
    storep t0, [cfr, t1, 8]
    dispatch(7)

.opGetByPnameSlow:
    callSlowPath(_llint_slow_path_get_by_pname)
    dispatch(7)


_llint_op_put_by_val:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadConstantOrVariableCell(t0, t1, .opPutByValSlow)
    loadis 16[PB, PC, 8], t0
    loadConstantOrVariableInt32(t0, t2, .opPutByValSlow)
    sxi2p t2, t2
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalData[t0], t0
    loadp JSGlobalData::jsArrayClassInfo[t0], t0
    bpneq [t1], t0, .opPutByValSlow
    biaeq t2, JSArray::m_vectorLength[t1], .opPutByValSlow
    loadp JSArray::m_storage[t1], t0
    btpz ArrayStorage::m_vector[t0, t2, 8], .opPutByValEmpty
.opPutByValStoreResult:
    loadis 24[PB, PC, 8], t3
    loadConstantOrVariable(t3, t1)
    writeBarrier(t1)
    storep t1, ArrayStorage::m_vector[t0, t2, 8]
    dispatch(4)

.opPutByValEmpty:
    addi 1, ArrayStorage::m_numValuesInVector[t0]
    bib t2, ArrayStorage::m_length[t0], .opPutByValStoreResult
    addi 1, t2, t1
    storei t1, ArrayStorage::m_length[t0]
    jmp .opPutByValStoreResult

.opPutByValSlow:
    callSlowPath(_llint_slow_path_put_by_val)
    dispatch(4)


_llint_op_loop:
    nop
_llint_op_jmp:
    traceExecution()
    dispatchInt(8[PB, PC, 8])


macro jumpTrueOrFalse(conditionOp, slow)
    loadis 8[PB, PC, 8], t1
    loadConstantOrVariable(t1, t0)
    xorp ValueFalse, t0
    btpnz t0, -1, .slow
    conditionOp(t0, .target)
    dispatch(3)

.target:
    dispatchInt(16[PB, PC, 8])

.slow:
    callSlowPath(slow)
    dispatch(0)
end


macro equalNull(cellHandler, immediateHandler)
    loadis 8[PB, PC, 8], t0
    assertNotConstant(t0)
    loadp [cfr, t0, 8], t0
    btpnz t0, tagMask, .immediate
    loadp JSCell::m_structure[t0], t2
    cellHandler(Structure::m_typeInfo + TypeInfo::m_flags[t2], .target)
    dispatch(3)

.target:
    dispatch(16[PB, PC, 8])

.immediate:
    andp ~TagBitUndefined, t0
    immediateHandler(t0, .target)
    dispatch(3)
end

_llint_op_jeq_null:
    traceExecution()
    equalNull(
        macro (value, target) btbnz value, MasqueradesAsUndefined, target end,
        macro (value, target) bpeq value, ValueNull, target end)


_llint_op_jneq_null:
    traceExecution()
    equalNull(
        macro (value, target) btbz value, MasqueradesAsUndefined, target end,
        macro (value, target) bpneq value, ValueNull, target end)


_llint_op_jneq_ptr:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadp 16[PB, PC, 8], t1
    bpneq t1, [cfr, t0, 8], .opJneqPtrTarget
    dispatch(4)

.opJneqPtrTarget:
    dispatchInt(24[PB, PC, 8])


macro compare(integerCompare, doubleCompare, slowPath)
    loadis 8[PB, PC, 8], t2
    loadis 16[PB, PC, 8], t3
    loadConstantOrVariable(t2, t0)
    loadConstantOrVariable(t3, t1)
    bpb t0, tagTypeNumber, .op1NotInt
    bpb t1, tagTypeNumber, .op2NotInt
    integerCompare(t0, t1, .jumpTarget)
    dispatch(4)

.op1NotInt:
    btpz t0, tagTypeNumber, .slow
    bpb t1, tagTypeNumber, .op1NotIntOp2NotInt
    ci2d t1, ft1
    jmp .op1NotIntReady
.op1NotIntOp2NotInt:
    btpz t1, tagTypeNumber, .slow
    addp tagTypeNumber, t1
    fp2d t1, ft1
.op1NotIntReady:
    addp tagTypeNumber, t0
    fp2d t0, ft0
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.op2NotInt:
    ci2d t0, ft0
    btpz t1, tagTypeNumber, .slow
    addp tagTypeNumber, t1
    fp2d t1, ft1
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.jumpTarget:
    dispatchInt(24[PB, PC, 8])

.slow:
    callSlowPath(slowPath)
    dispatch(0)
end


_llint_op_switch_imm:
    traceExecution()
    loadis 24[PB, PC, 8], t2
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariable(t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3    # FIXME: would be nice to peephole this!
    loadp CodeBlock::RareData::m_immediateSwitchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bpb t1, tagTypeNumber, .opSwitchImmNotInt
    subi SimpleJumpTable::min[t2], t1
    biaeq t1, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchImmFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t3
    loadis [t3, t1, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatch(t1)

.opSwitchImmNotInt:
    btpnz t1, tagTypeNumber, .opSwitchImmSlow   # Go slow if it's a double.
.opSwitchImmFallThrough:
    dispatchInt(16[PB, PC, 8])

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    dispatch(0)


_llint_op_switch_char:
    traceExecution()
    loadis 24[PB, PC, 8], t2
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariable(t2, t1)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3
    loadp CodeBlock::RareData::m_characterSwitchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    btpnz t1, tagMask, .opSwitchCharFallThrough
    loadp JSCell::m_structure[t1], t0
    bbneq Structure::m_typeInfo + TypeInfo::m_type[t0], StringType, .opSwitchCharFallThrough
    bineq JSString::m_length[t1], 1, .opSwitchCharFallThrough
    loadp JSString::m_value[t1], t0
    btpz  t0, .opSwitchOnRope
    loadp StringImpl::m_data8[t0], t1
    btinz StringImpl::m_hashAndFlags[t0], HashFlags8BitBuffer, .opSwitchChar8Bit
    loadh [t1], t0
    jmp .opSwitchCharReady
.opSwitchChar8Bit:
    loadb [t1], t0
.opSwitchCharReady:
    subi SimpleJumpTable::min[t2], t0
    biaeq t0, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchCharFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t2
    loadis [t2, t0, 4], t1
    btiz t1, .opSwitchCharFallThrough
    dispatch(t1)

.opSwitchCharFallThrough:
    dispatchInt(16[PB, PC, 8])

.opSwitchOnRope:
    callSlowPath(_llint_slow_path_switch_char)
    dispatch(0)


_llint_op_new_func:
    traceExecution()
    btiz 24[PB, PC, 8], .opNewFuncUnchecked
    loadis 8[PB, PC, 8], t1
    btpnz [cfr, t1, 8], .opNewFuncDone
.opNewFuncUnchecked:
    callSlowPath(_llint_slow_path_new_func)
.opNewFuncDone:
    dispatch(4)


macro doCall(slowPath)
    loadis 8[PB, PC, 8], t0
    loadp 32[PB, PC, 8], t1
    loadp LLIntCallLinkInfo::callee[t1], t2
    loadConstantOrVariable(t0, t3)
    bpneq t3, t2, .opCallSlow
    loadis 24[PB, PC, 8], t3
    addi 6, PC
    lshifti 3, t3
    addp cfr, t3
    loadp JSFunction::m_scopeChain[t2], t0
    storep t2, Callee[t3]
    storep t0, ScopeChain[t3]
    loadis 16 - 48[PB, PC, 8], t2
    storei PC, ArgumentCount + TagOffset[cfr]
    storep cfr, CallerFrame[t3]
    storei t2, ArgumentCount + PayloadOffset[t3]
    move t3, cfr
    call LLIntCallLinkInfo::machineCodeTarget[t1]
    dispatchAfterCall()

.opCallSlow:
    slowPathForCall(6, slowPath)
end


_llint_op_tear_off_activation:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    loadis 16[PB, PC, 8], t1
    btpnz [cfr, t0, 8], .opTearOffActivationCreated
    btpz [cfr, t1, 8], .opTearOffActivationNotCreated
.opTearOffActivationCreated:
    callSlowPath(_llint_slow_path_tear_off_activation)
.opTearOffActivationNotCreated:
    dispatch(3)


_llint_op_tear_off_arguments:
    traceExecution()
    loadis 8[PB, PC, 8], t0
    subi 1, t0   # Get the unmodifiedArgumentsRegister
    btpz [cfr, t0, 8], .opTearOffArgumentsNotCreated
    callSlowPath(_llint_slow_path_tear_off_arguments)
.opTearOffArgumentsNotCreated:
    dispatch(2)


_llint_op_ret:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t2, t0)
    doReturn()


_llint_op_call_put_result:
    loadis 8[PB, PC, 8], t2
    loadp 16[PB, PC, 8], t3
    storep t0, [cfr, t2, 8]
    valueProfile(t0, t3)
    traceExecution()
    dispatch(3)


_llint_op_ret_object_or_this:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadis 8[PB, PC, 8], t2
    loadConstantOrVariable(t2, t0)
    btpnz t0, tagMask, .opRetObjectOrThisNotObject
    loadp JSCell::m_structure[t0], t2
    bbb Structure::m_typeInfo + TypeInfo::m_type[t2], ObjectType, .opRetObjectOrThisNotObject
    doReturn()

.opRetObjectOrThisNotObject:
    loadis 16[PB, PC, 8], t2
    loadConstantOrVariable(t2, t0)
    doReturn()


_llint_op_to_primitive:
    traceExecution()
    loadis 16[PB, PC, 8], t2
    loadis 8[PB, PC, 8], t3
    loadConstantOrVariable(t2, t0)
    btpnz t0, tagMask, .opToPrimitiveIsImm
    loadp JSCell::m_structure[t0], t2
    bbneq Structure::m_typeInfo + TypeInfo::m_type[t2], StringType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    storep t0, [cfr, t3, 8]
    dispatch(3)

.opToPrimitiveSlowCase:
    callSlowPath(_llint_slow_path_to_primitive)
    dispatch(3)


_llint_op_next_pname:
    traceExecution()
    loadis 24[PB, PC, 8], t1
    loadis 32[PB, PC, 8], t2
    assertNotConstant(t1)
    assertNotConstant(t2)
    loadi PayloadOffset[cfr, t1, 8], t0
    bieq t0, PayloadOffset[cfr, t2, 8], .opNextPnameEnd
    loadis 40[PB, PC, 8], t2
    assertNotConstant(t2)
    loadp [cfr, t2, 8], t2
    loadp JSPropertyNameIterator::m_jsStrings[t2], t3
    loadp [t3, t0, 8], t3
    addi 1, t0
    storei t0, PayloadOffset[cfr, t1, 8]
    loadis 8[PB, PC, 8], t1
    storep t3, [cfr, t1, 8]
    loadis 16[PB, PC, 8], t3
    assertNotConstant(t3)
    loadp [cfr, t3, 8], t3
    loadp JSCell::m_structure[t3], t1
    bpneq t1, JSPropertyNameIterator::m_cachedStructure[t2], .opNextPnameSlow
    loadp JSPropertyNameIterator::m_cachedPrototypeChain[t2], t0
    loadp StructureChain::m_vector[t0], t0
    btpz [t0], .opNextPnameTarget
.opNextPnameCheckPrototypeLoop:
    bpeq Structure::m_prototype[t1], ValueNull, .opNextPnameSlow
    loadp Structure::m_prototype[t1], t2
    loadp JSCell::m_structure[t2], t1
    bpneq t1, [t0], .opNextPnameSlow
    addp 8, t0
    btpnz [t0], .opNextPnameCheckPrototypeLoop
.opNextPnameTarget:
    dispatchInt(48[PB, PC, 8])

.opNextPnameEnd:
    dispatch(7)

.opNextPnameSlow:
    callSlowPath(_llint_slow_path_next_pname) # This either keeps the PC where it was (causing us to loop) or sets it to target.
    dispatch(0)


_llint_op_catch:
    # This is where we end up from the JIT's throw trampoline (because the
    # machine code return address will be set to _llint_op_catch), and from
    # the interpreter's throw trampoline (see _llint_throw_trampoline).
    # The JIT throwing protocol calls for the cfr to be in t0. The throwing
    # code must have known that we were throwing to the interpreter, and have
    # set JSGlobalData::targetInterpreterPCForThrow.
    move t0, cfr
    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructions[PB], PB
    loadp JITStackFrame::globalData[sp], t3
    loadp JSGlobalData::targetInterpreterPCForThrow[t3], PC
    subp PB, PC
    urshiftp 3, PC
    loadp JSGlobalData::exception[t3], t0
    storep 0, JSGlobalData::exception[t3]
    loadis 8[PB, PC, 8], t2
    storep t0, [cfr, t2, 8]
    traceExecution()
    dispatch(2)


_llint_op_end:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadis 8[PB, PC, 8], t0
    assertNotConstant(t0)
    loadp [cfr, t0, 8], t0
    doReturn()


_llint_throw_from_slow_path_trampoline:
    # When throwing from the interpreter (i.e. throwing from LLIntSlowPaths), so
    # the throw target is not necessarily interpreted code, we come to here.
    # This essentially emulates the JIT's throwing protocol.
    loadp JITStackFrame::globalData[sp], t1
    loadp JSGlobalData::callFrameForThrow[t1], t0
    jmp JSGlobalData::targetMachinePCForThrow[t1]


_llint_throw_during_call_trampoline:
    preserveReturnAddressAfterCall(t2)
    loadp JITStackFrame::globalData[sp], t1
    loadp JSGlobalData::callFrameForThrow[t1], t0
    jmp JSGlobalData::targetMachinePCForThrow[t1]


macro nativeCallTrampoline(executableOffsetToFunction)
    storep 0, CodeBlock[cfr]
    loadp JITStackFrame::globalData + 8[sp], t0
    storep cfr, JSGlobalData::topCallFrame[t0]
    loadp CallerFrame[cfr], t0
    loadp ScopeChain[t0], t1
    storep t1, ScopeChain[cfr]
    peek 0, t1
    storep t1, ReturnPC[cfr]
    move cfr, t5  # t5 = rdi
    subp 16 - 8, sp
    loadp Callee[cfr], t4 # t4 = rsi
    loadp JSFunction::m_executable[t4], t1
    move t0, cfr # Restore cfr to avoid loading from stack
    call executableOffsetToFunction[t1]
    addp 16 - 8, sp
    loadp JITStackFrame::globalData + 8[sp], t3
    btpnz JSGlobalData::exception[t3], .exception
    ret
.exception:
    preserveReturnAddressAfterCall(t1)
    loadi ArgumentCount + TagOffset[cfr], PC
    loadp CodeBlock[cfr], PB
    loadp CodeBlock::m_instructions[PB], PB
    loadp JITStackFrame::globalData[sp], t0
    storep cfr, JSGlobalData::topCallFrame[t0]
    callSlowPath(_llint_throw_from_native_call)
    jmp _llint_throw_from_slow_path_trampoline
end

