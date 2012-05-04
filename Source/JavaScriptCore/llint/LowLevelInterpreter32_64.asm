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


# Crash course on the language that this is written in (which I just call
# "assembly" even though it's more than that):
#
# - Mostly gas-style operand ordering. The last operand tends to be the
#   destination. So "a := b" is written as "mov b, a". But unlike gas,
#   comparisons are in-order, so "if (a < b)" is written as
#   "bilt a, b, ...".
#
# - "b" = byte, "h" = 16-bit word, "i" = 32-bit word, "p" = pointer.
#   Currently this is just 32-bit so "i" and "p" are interchangeable
#   except when an op supports one but not the other.
#
# - In general, valid operands for macro invocations and instructions are
#   registers (eg "t0"), addresses (eg "4[t0]"), base-index addresses
#   (eg "7[t0, t1, 2]"), absolute addresses (eg "0xa0000000[]"), or labels
#   (eg "_foo" or ".foo"). Macro invocations can also take anonymous
#   macros as operands. Instructions cannot take anonymous macros.
#
# - Labels must have names that begin with either "_" or ".".  A "." label
#   is local and gets renamed before code gen to minimize namespace
#   pollution. A "_" label is an extern symbol (i.e. ".globl"). The "_"
#   may or may not be removed during code gen depending on whether the asm
#   conventions for C name mangling on the target platform mandate a "_"
#   prefix.
#
# - A "macro" is a lambda expression, which may be either anonymous or
#   named. But this has caveats. "macro" can take zero or more arguments,
#   which may be macros or any valid operands, but it can only return
#   code. But you can do Turing-complete things via continuation passing
#   style: "macro foo (a, b) b(a) end foo(foo, foo)". Actually, don't do
#   that, since you'll just crash the assembler.
#
# - An "if" is a conditional on settings. Any identifier supplied in the
#   predicate of an "if" is assumed to be a #define that is available
#   during code gen. So you can't use "if" for computation in a macro, but
#   you can use it to select different pieces of code for different
#   platforms.
#
# - Arguments to macros follow lexical scoping rather than dynamic scoping.
#   Const's also follow lexical scoping and may override (hide) arguments
#   or other consts. All variables (arguments and constants) can be bound
#   to operands. Additionally, arguments (but not constants) can be bound
#   to macros.


# Below we have a bunch of constant declarations. Each constant must have
# a corresponding ASSERT() in LLIntData.cpp.


# Value representation constants.
const Int32Tag = -1
const BooleanTag = -2
const NullTag = -3
const UndefinedTag = -4
const CellTag = -5
const EmptyValueTag = -6
const DeletedValueTag = -7
const LowestTag = DeletedValueTag


# Utilities
macro dispatch(advance)
    addp advance * 4, PC
    jmp [PC]
end

macro dispatchBranchWithOffset(pcOffset)
    lshifti 2, pcOffset
    addp pcOffset, PC
    jmp [PC]
end

macro dispatchBranch(pcOffset)
    loadi pcOffset, t0
    dispatchBranchWithOffset(t0)
end

macro dispatchAfterCall()
    loadi ArgumentCount + TagOffset[cfr], PC
    jmp [PC]
end

macro cCall2(function, arg1, arg2)
    if ARMv7
        move arg1, t0
        move arg2, t1
    elsif X86
        poke arg1, 0
        poke arg2, 1
    else
        error
    end
    call function
end

# This barely works. arg3 and arg4 should probably be immediates.
macro cCall4(function, arg1, arg2, arg3, arg4)
    if ARMv7
        move arg1, t0
        move arg2, t1
        move arg3, t2
        move arg4, t3
    elsif X86
        poke arg1, 0
        poke arg2, 1
        poke arg3, 2
        poke arg4, 3
    else
        error
    end
    call function
end

macro callSlowPath(slowPath)
    cCall2(slowPath, cfr, PC)
    move t0, PC
    move t1, cfr
end

# Debugging operation if you'd like to print an operand in the instruction stream. fromWhere
# should be an immediate integer - any integer you like; use it to identify the place you're
# debugging from. operand should likewise be an immediate, and should identify the operand
# in the instruction stream you'd like to print out.
macro traceOperand(fromWhere, operand)
    cCall4(_llint_trace_operand, cfr, PC, fromWhere, operand)
    move t0, PC
    move t1, cfr
end

# Debugging operation if you'd like to print the value of an operand in the instruction
# stream. Same as traceOperand(), but assumes that the operand is a register, and prints its
# value.
macro traceValue(fromWhere, operand)
    cCall4(_llint_trace_value, cfr, PC, fromWhere, operand)
    move t0, PC
    move t1, cfr
end

# Call a slowPath for call opcodes.
macro callCallSlowPath(advance, slowPath, action)
    addp advance * 4, PC, t0
    storep t0, ArgumentCount + TagOffset[cfr]
    cCall2(slowPath, cfr, PC)
    move t1, cfr
    action(t0)
end

macro checkSwitchToJITForLoop()
    checkSwitchToJIT(
        1,
        macro ()
            storei PC, ArgumentCount + TagOffset[cfr]
            cCall2(_llint_loop_osr, cfr, PC)
            move t1, cfr
            btpz t0, .recover
            jmp t0
        .recover:
            loadi ArgumentCount + TagOffset[cfr], PC
        end)
end

# Index, tag, and payload must be different registers. Index is not
# changed.
macro loadConstantOrVariable(index, tag, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], payload
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    loadp TagOffset[payload, index, 8], tag
    loadp PayloadOffset[payload, index, 8], payload
.done:
end

macro loadConstantOrVariableTag(index, tag)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    jmp .done
.constant:
    loadp CodeBlock[cfr], tag
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    loadp TagOffset[tag, index, 8], tag
.done:
end

# Index and payload may be the same register. Index may be clobbered.
macro loadConstantOrVariable2Reg(index, tag, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    loadi TagOffset[cfr, index, 8], tag
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], tag
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[tag], tag
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    lshifti 3, index
    addp index, tag
    loadp PayloadOffset[tag], payload
    loadp TagOffset[tag], tag
.done:
end

macro loadConstantOrVariablePayloadTagCustom(index, tagCheck, payload)
    bigteq index, FirstConstantRegisterIndex, .constant
    tagCheck(TagOffset[cfr, index, 8])
    loadi PayloadOffset[cfr, index, 8], payload
    jmp .done
.constant:
    loadp CodeBlock[cfr], payload
    loadp CodeBlock::m_constantRegisters + VectorBufferOffset[payload], payload
    # There is a bit of evil here: if the index contains a value >= FirstConstantRegisterIndex,
    # then value << 3 will be equal to (value - FirstConstantRegisterIndex) << 3.
    tagCheck(TagOffset[payload, index, 8])
    loadp PayloadOffset[payload, index, 8], payload
.done:
end

# Index and payload must be different registers. Index is not mutated. Use
# this if you know what the tag of the variable should be. Doing the tag
# test as part of loading the variable reduces register use, but may not
# be faster than doing loadConstantOrVariable followed by a branch on the
# tag.
macro loadConstantOrVariablePayload(index, expectedTag, payload, slow)
    loadConstantOrVariablePayloadTagCustom(
        index,
        macro (actualTag) bineq actualTag, expectedTag, slow end,
        payload)
end

macro loadConstantOrVariablePayloadUnchecked(index, payload)
    loadConstantOrVariablePayloadTagCustom(
        index,
        macro (actualTag) end,
        payload)
end

macro writeBarrier(tag, payload)
    # Nothing to do, since we don't have a generational or incremental collector.
end

macro valueProfile(tag, payload, profile)
    if VALUE_PROFILER
        storei tag, ValueProfile::m_buckets + TagOffset[profile]
        storei payload, ValueProfile::m_buckets + PayloadOffset[profile]
    end
end


# Entrypoints into the interpreter

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
macro functionArityCheck(doneLabel, slow_path)
    loadi PayloadOffset + ArgumentCount[cfr], t0
    biaeq t0, CodeBlock::m_numParameters[t1], doneLabel
    cCall2(slow_path, cfr, PC)   # This slow_path has a simple protocol: t0 = 0 => no error, t0 != 0 => error
    move t1, cfr
    btiz t0, .continue
    loadp JITStackFrame::globalData[sp], t1
    loadp JSGlobalData::callFrameForThrow[t1], t0
    jmp JSGlobalData::targetMachinePCForThrow[t1]
.continue:
    # Reload CodeBlock and PC, since the slow_path clobbered it.
    loadp CodeBlock[cfr], t1
    loadp CodeBlock::m_instructions[t1], PC
    jmp doneLabel
end


# Instruction implementations

_llint_op_enter:
    traceExecution()
    loadp CodeBlock[cfr], t2
    loadi CodeBlock::m_numVars[t2], t2
    btiz t2, .opEnterDone
    move UndefinedTag, t0
    move 0, t1
.opEnterLoop:
    subi 1, t2
    storei t0, TagOffset[cfr, t2, 8]
    storei t1, PayloadOffset[cfr, t2, 8]
    btinz t2, .opEnterLoop
.opEnterDone:
    dispatch(1)


_llint_op_create_activation:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opCreateActivationDone
    callSlowPath(_llint_slow_path_create_activation)
.opCreateActivationDone:
    dispatch(2)


_llint_op_init_lazy_reg:
    traceExecution()
    loadi 4[PC], t0
    storei EmptyValueTag, TagOffset[cfr, t0, 8]
    storei 0, PayloadOffset[cfr, t0, 8]
    dispatch(2)


_llint_op_create_arguments:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opCreateArgumentsDone
    callSlowPath(_llint_slow_path_create_arguments)
.opCreateArgumentsDone:
    dispatch(2)


_llint_op_create_this:
    traceExecution()
    loadi 8[PC], t0
    assertNotConstant(t0)
    bineq TagOffset[cfr, t0, 8], CellTag, .opCreateThisSlow
    loadi PayloadOffset[cfr, t0, 8], t0
    loadp JSCell::m_structure[t0], t1
    bbb Structure::m_typeInfo + TypeInfo::m_type[t1], ObjectType, .opCreateThisSlow
    loadp JSObject::m_inheritorID[t0], t2
    btpz t2, .opCreateThisSlow
    allocateBasicJSObject(JSFinalObjectSizeClassIndex, JSGlobalData::jsFinalObjectClassInfo, t2, t0, t1, t3, .opCreateThisSlow)
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
    dispatch(3)

.opCreateThisSlow:
    callSlowPath(_llint_slow_path_create_this)
    dispatch(3)


_llint_op_get_callee:
    traceExecution()
    loadi 4[PC], t0
    loadp PayloadOffset + Callee[cfr], t1
    storei CellTag, TagOffset[cfr, t0, 8]
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(2)


_llint_op_convert_this:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], CellTag, .opConvertThisSlow
    loadi PayloadOffset[cfr, t0, 8], t0
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
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
    dispatch(2)

.opNewObjectSlow:
    callSlowPath(_llint_slow_path_new_object)
    dispatch(2)


_llint_op_mov:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    storei t2, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(3)


_llint_op_not:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    loadConstantOrVariable(t0, t2, t3)
    bineq t2, BooleanTag, .opNotSlow
    xori 1, t3
    storei t2, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    dispatch(3)

.opNotSlow:
    callSlowPath(_llint_slow_path_not)
    dispatch(3)


_llint_op_eq:
    traceExecution()
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .opEqSlow
    bieq t2, CellTag, .opEqSlow
    bib t2, LowestTag, .opEqSlow
    loadi 4[PC], t2
    cieq t0, t1, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.opEqSlow:
    callSlowPath(_llint_slow_path_eq)
    dispatch(4)


_llint_op_eq_null:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    bineq t1, CellTag, .opEqNullImmediate
    loadp JSCell::m_structure[t0], t1
    tbnz Structure::m_typeInfo + TypeInfo::m_flags[t1], MasqueradesAsUndefined, t1
    jmp .opEqNullNotImmediate
.opEqNullImmediate:
    cieq t1, NullTag, t2
    cieq t1, UndefinedTag, t1
    ori t2, t1
.opEqNullNotImmediate:
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    dispatch(3)


_llint_op_neq:
    traceExecution()
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .opNeqSlow
    bieq t2, CellTag, .opNeqSlow
    bib t2, LowestTag, .opNeqSlow
    loadi 4[PC], t2
    cineq t0, t1, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.opNeqSlow:
    callSlowPath(_llint_slow_path_neq)
    dispatch(4)
    

_llint_op_neq_null:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    bineq t1, CellTag, .opNeqNullImmediate
    loadp JSCell::m_structure[t0], t1
    tbz Structure::m_typeInfo + TypeInfo::m_flags[t1], MasqueradesAsUndefined, t1
    jmp .opNeqNullNotImmediate
.opNeqNullImmediate:
    cineq t1, NullTag, t2
    cineq t1, UndefinedTag, t1
    andi t2, t1
.opNeqNullNotImmediate:
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    dispatch(3)


macro strictEq(equalityOperation, slowPath)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, t3, .slow
    bib t2, LowestTag, .slow
    bineq t2, CellTag, .notString
    loadp JSCell::m_structure[t0], t2
    loadp JSCell::m_structure[t1], t3
    bbneq Structure::m_typeInfo + TypeInfo::m_type[t2], StringType, .notString
    bbeq Structure::m_typeInfo + TypeInfo::m_type[t3], StringType, .slow
.notString:
    loadi 4[PC], t2
    equalityOperation(t0, t1, t0)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(4)

.slow:
    callSlowPath(slowPath)
    dispatch(4)
end

_llint_op_stricteq:
    traceExecution()
    strictEq(macro (left, right, result) cieq left, right, result end, _llint_slow_path_stricteq)


_llint_op_nstricteq:
    traceExecution()
    strictEq(macro (left, right, result) cineq left, right, result end, _llint_slow_path_nstricteq)


_llint_op_pre_inc:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opPreIncSlow
    loadi PayloadOffset[cfr, t0, 8], t1
    baddio 1, t1, .opPreIncSlow
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(2)

.opPreIncSlow:
    callSlowPath(_llint_slow_path_pre_inc)
    dispatch(2)


_llint_op_pre_dec:
    traceExecution()
    loadi 4[PC], t0
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opPreDecSlow
    loadi PayloadOffset[cfr, t0, 8], t1
    bsubio 1, t1, .opPreDecSlow
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(2)

.opPreDecSlow:
    callSlowPath(_llint_slow_path_pre_dec)
    dispatch(2)


_llint_op_post_inc:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opPostIncSlow
    bieq t0, t1, .opPostIncDone
    loadi PayloadOffset[cfr, t0, 8], t2
    move t2, t3
    baddio 1, t3, .opPostIncSlow
    storei Int32Tag, TagOffset[cfr, t1, 8]
    storei t2, PayloadOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
.opPostIncDone:
    dispatch(3)

.opPostIncSlow:
    callSlowPath(_llint_slow_path_post_inc)
    dispatch(3)


_llint_op_post_dec:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    bineq TagOffset[cfr, t0, 8], Int32Tag, .opPostDecSlow
    bieq t0, t1, .opPostDecDone
    loadi PayloadOffset[cfr, t0, 8], t2
    move t2, t3
    bsubio 1, t3, .opPostDecSlow
    storei Int32Tag, TagOffset[cfr, t1, 8]
    storei t2, PayloadOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
.opPostDecDone:
    dispatch(3)

.opPostDecSlow:
    callSlowPath(_llint_slow_path_post_dec)
    dispatch(3)


_llint_op_to_jsnumber:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    loadConstantOrVariable(t0, t2, t3)
    bieq t2, Int32Tag, .opToJsnumberIsInt
    biaeq t2, EmptyValueTag, .opToJsnumberSlow
.opToJsnumberIsInt:
    storei t2, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    dispatch(3)

.opToJsnumberSlow:
    callSlowPath(_llint_slow_path_to_jsnumber)
    dispatch(3)


_llint_op_negate:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t3
    loadConstantOrVariable(t0, t1, t2)
    bineq t1, Int32Tag, .opNegateSrcNotInt
    btiz t2, 0x7fffffff, .opNegateSlow
    negi t2
    storei Int32Tag, TagOffset[cfr, t3, 8]
    storei t2, PayloadOffset[cfr, t3, 8]
    dispatch(3)
.opNegateSrcNotInt:
    bia t1, LowestTag, .opNegateSlow
    xori 0x80000000, t1
    storei t1, TagOffset[cfr, t3, 8]
    storei t2, PayloadOffset[cfr, t3, 8]
    dispatch(3)

.opNegateSlow:
    callSlowPath(_llint_slow_path_negate)
    dispatch(3)


macro binaryOpCustomStore(integerOperationAndStore, doubleOperation, slowPath)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t2, Int32Tag, .op1NotInt
    bineq t3, Int32Tag, .op2NotInt
    loadi 4[PC], t2
    integerOperationAndStore(t3, t1, t0, .slow, t2)
    dispatch(5)

.op1NotInt:
    # First operand is definitely not an int, the second operand could be anything.
    bia t2, LowestTag, .slow
    bib t3, LowestTag, .op1NotIntOp2Double
    bineq t3, Int32Tag, .slow
    ci2d t1, ft1
    jmp .op1NotIntReady
.op1NotIntOp2Double:
    fii2d t1, t3, ft1
.op1NotIntReady:
    loadi 4[PC], t1
    fii2d t0, t2, ft0
    doubleOperation(ft1, ft0)
    stored ft0, [cfr, t1, 8]
    dispatch(5)

.op2NotInt:
    # First operand is definitely an int, the second operand is definitely not.
    loadi 4[PC], t2
    bia t3, LowestTag, .slow
    ci2d t0, ft0
    fii2d t1, t3, ft1
    doubleOperation(ft1, ft0)
    stored ft0, [cfr, t2, 8]
    dispatch(5)

.slow:
    callSlowPath(slowPath)
    dispatch(5)
end

macro binaryOp(integerOperation, doubleOperation, slowPath)
    binaryOpCustomStore(
        macro (int32Tag, left, right, slow, index)
            integerOperation(left, right, slow)
            storei int32Tag, TagOffset[cfr, index, 8]
            storei right, PayloadOffset[cfr, index, 8]
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
        macro (int32Tag, left, right, slow, index)
            const scratch = int32Tag   # We know that we can reuse the int32Tag register since it has a constant.
            move right, scratch
            bmulio left, scratch, slow
            btinz scratch, .done
            bilt left, 0, slow
            bilt right, 0, slow
        .done:
            storei Int32Tag, TagOffset[cfr, index, 8]
            storei scratch, PayloadOffset[cfr, index, 8]
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
        macro (int32Tag, left, right, slow, index)
            ci2d left, ft0
            ci2d right, ft1
            divd ft0, ft1
            bcd2i ft1, right, .notInt
            storei int32Tag, TagOffset[cfr, index, 8]
            storei right, PayloadOffset[cfr, index, 8]
            jmp .done
        .notInt:
            stored ft1, [cfr, index, 8]
        .done:
        end,
        macro (left, right) divd left, right end,
        _llint_slow_path_div)


macro bitOp(operation, slowPath, advance)
    loadi 12[PC], t2
    loadi 8[PC], t0
    loadConstantOrVariable(t2, t3, t1)
    loadConstantOrVariable2Reg(t0, t2, t0)
    bineq t3, Int32Tag, .slow
    bineq t2, Int32Tag, .slow
    loadi 4[PC], t2
    operation(t1, t0, .slow)
    storei t3, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
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
    loadi 4[PC], t1
    loadConstantOrVariablePayload(t1, CellTag, t0, .opCheckHasInstanceSlow)
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
    loadi 12[PC], t1
    loadConstantOrVariablePayloadUnchecked(t1, t0)
    loadp JSCell::m_structure[t0], t0
    btbz Structure::m_typeInfo + TypeInfo::m_flags[t0], ImplementsDefaultHasInstance, .opInstanceofSlow
    
    # Actually do the work.
    loadi 16[PC], t0
    loadi 4[PC], t3
    loadConstantOrVariablePayload(t0, CellTag, t1, .opInstanceofSlow)
    loadp JSCell::m_structure[t1], t2
    bbb Structure::m_typeInfo + TypeInfo::m_type[t2], ObjectType, .opInstanceofSlow
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t2, .opInstanceofSlow)
    
    # Register state: t1 = prototype, t2 = value
    move 1, t0
.opInstanceofLoop:
    loadp JSCell::m_structure[t2], t2
    loadi Structure::m_prototype + PayloadOffset[t2], t2
    bpeq t2, t1, .opInstanceofDone
    btinz t2, .opInstanceofLoop

    move 0, t0
.opInstanceofDone:
    storei BooleanTag, TagOffset[cfr, t3, 8]
    storei t0, PayloadOffset[cfr, t3, 8]
    dispatch(5)

.opInstanceofSlow:
    callSlowPath(_llint_slow_path_instanceof)
    dispatch(5)


_llint_op_is_undefined:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    storei BooleanTag, TagOffset[cfr, t0, 8]
    bieq t2, CellTag, .opIsUndefinedCell
    cieq t2, UndefinedTag, t3
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(3)
.opIsUndefinedCell:
    loadp JSCell::m_structure[t3], t1
    tbnz Structure::m_typeInfo + TypeInfo::m_flags[t1], MasqueradesAsUndefined, t1
    storei t1, PayloadOffset[cfr, t0, 8]
    dispatch(3)


_llint_op_is_boolean:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariableTag(t1, t0)
    cieq t0, BooleanTag, t0
    storei BooleanTag, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    dispatch(3)


_llint_op_is_number:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariableTag(t1, t0)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    addi 1, t0
    cib t0, LowestTag + 1, t1
    storei t1, PayloadOffset[cfr, t2, 8]
    dispatch(3)


_llint_op_is_string:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t2
    loadConstantOrVariable(t1, t0, t3)
    storei BooleanTag, TagOffset[cfr, t2, 8]
    bineq t0, CellTag, .opIsStringNotCell
    loadp JSCell::m_structure[t3], t0
    cbeq Structure::m_typeInfo + TypeInfo::m_type[t0], StringType, t1
    storei t1, PayloadOffset[cfr, t2, 8]
    dispatch(3)
.opIsStringNotCell:
    storep 0, PayloadOffset[cfr, t2, 8]
    dispatch(3)


macro resolveGlobal(size, slow)
    # Operands are as follows:
    # 4[PC]   Destination for the load.
    # 8[PC]   Property identifier index in the code block.
    # 12[PC]  Structure pointer, initialized to 0 by bytecode generator.
    # 16[PC]  Offset in global object, initialized to 0 by bytecode generator.
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSCell::m_structure[t0], t1
    bpneq t1, 12[PC], slow
    loadi 16[PC], t1
    loadp JSObject::m_propertyStorage[t0], t0
    loadi TagOffset[t0, t1, 8], t2
    loadi PayloadOffset[t0, t1, 8], t3
    loadi 4[PC], t0
    storei t2, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    loadi (size - 1) * 4[PC], t0
    valueProfile(t2, t3, t0)
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
    loadp ScopeChain + PayloadOffset[cfr], t0
    loadi deBruijinIndexOperand, t2
    
    btiz t2, .done
    
    loadp CodeBlock[cfr], t1
    bineq CodeBlock::m_codeType[t1], FunctionCode, .loop
    btbz CodeBlock::m_needsFullScopeChain[t1], .loop
    
    loadi CodeBlock::m_activationRegister[t1], t1

    # Need to conditionally skip over one scope.
    bieq TagOffset[cfr, t1, 8], EmptyValueTag, .noActivation
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
        20[PC],
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
    # 4[PC]   Destination for the load.
    # 8[PC]   Index of register in the scope.
    # 12[PC]  De Bruijin index.
    getScope(12[PC], macro (scope, scratch) end)
    loadi 4[PC], t1
    loadi 8[PC], t2
    loadp ScopeChainNode::object[t0], t0
    loadp JSVariableObject::m_registers[t0], t0
    loadi TagOffset[t0, t2, 8], t3
    loadi PayloadOffset[t0, t2, 8], t0
    storei t3, TagOffset[cfr, t1, 8]
    storei t0, PayloadOffset[cfr, t1, 8]
    loadi 16[PC], t1
    valueProfile(t3, t0, t1)
    dispatch(5)


_llint_op_put_scoped_var:
    traceExecution()
    getScope(8[PC], macro (scope, scratch) end)
    loadi 12[PC], t1
    loadConstantOrVariable(t1, t3, t2)
    loadi 4[PC], t1
    writeBarrier(t3, t2)
    loadp ScopeChainNode::object[t0], t0
    loadp JSVariableObject::m_registers[t0], t0
    storei t3, TagOffset[t0, t1, 8]
    storei t2, PayloadOffset[t0, t1, 8]
    dispatch(4)


_llint_op_get_global_var:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t3
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_registers[t0], t0
    loadi TagOffset[t0, t1, 8], t2
    loadi PayloadOffset[t0, t1, 8], t1
    storei t2, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    loadi 12[PC], t3
    valueProfile(t2, t1, t3)
    dispatch(4)


_llint_op_put_global_var:
    traceExecution()
    loadi 8[PC], t1
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalObject[t0], t0
    loadp JSGlobalObject::m_registers[t0], t0
    loadConstantOrVariable(t1, t2, t3)
    loadi 4[PC], t1
    writeBarrier(t2, t3)
    storei t2, TagOffset[t0, t1, 8]
    storei t3, PayloadOffset[t0, t1, 8]
    dispatch(3)


_llint_op_get_by_id:
    traceExecution()
    # We only do monomorphic get_by_id caching for now, and we do not modify the
    # opcode. We do, however, allow for the cache to change anytime if fails, since
    # ping-ponging is free. At best we get lucky and the get_by_id will continue
    # to take fast path on the new cache. At worst we take slow path, which is what
    # we would have been doing anyway.
    loadi 8[PC], t0
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t0, CellTag, t3, .opGetByIdSlow)
    loadi 20[PC], t2
    loadp JSObject::m_propertyStorage[t3], t0
    bpneq JSCell::m_structure[t3], t1, .opGetByIdSlow
    loadi 4[PC], t1
    loadi TagOffset[t0, t2], t3
    loadi PayloadOffset[t0, t2], t2
    storei t3, TagOffset[cfr, t1, 8]
    storei t2, PayloadOffset[cfr, t1, 8]
    loadi 32[PC], t1
    valueProfile(t3, t2, t1)
    dispatch(9)

.opGetByIdSlow:
    callSlowPath(_llint_slow_path_get_by_id)
    dispatch(9)


_llint_op_get_arguments_length:
    traceExecution()
    loadi 8[PC], t0
    loadi 4[PC], t1
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opGetArgumentsLengthSlow
    loadi ArgumentCount + PayloadOffset[cfr], t2
    subi 1, t2
    storei Int32Tag, TagOffset[cfr, t1, 8]
    storei t2, PayloadOffset[cfr, t1, 8]
    dispatch(4)

.opGetArgumentsLengthSlow:
    callSlowPath(_llint_slow_path_get_arguments_length)
    dispatch(4)


_llint_op_put_by_id:
    traceExecution()
    loadi 4[PC], t3
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t3, CellTag, t0, .opPutByIdSlow)
    loadi 12[PC], t2
    loadp JSObject::m_propertyStorage[t0], t3
    bpneq JSCell::m_structure[t0], t1, .opPutByIdSlow
    loadi 20[PC], t1
    loadConstantOrVariable2Reg(t2, t0, t2)
    writeBarrier(t0, t2)
    storei t0, TagOffset[t3, t1]
    storei t2, PayloadOffset[t3, t1]
    dispatch(9)

.opPutByIdSlow:
    callSlowPath(_llint_slow_path_put_by_id)
    dispatch(9)


macro putByIdTransition(additionalChecks)
    traceExecution()
    loadi 4[PC], t3
    loadi 16[PC], t1
    loadConstantOrVariablePayload(t3, CellTag, t0, .opPutByIdSlow)
    loadi 12[PC], t2
    bpneq JSCell::m_structure[t0], t1, .opPutByIdSlow
    additionalChecks(t1, t3, .opPutByIdSlow)
    loadi 20[PC], t1
    loadp JSObject::m_propertyStorage[t0], t3
    addp t1, t3
    loadConstantOrVariable2Reg(t2, t1, t2)
    writeBarrier(t1, t2)
    storei t1, TagOffset[t3]
    loadi 24[PC], t1
    storei t2, PayloadOffset[t3]
    storep t1, JSCell::m_structure[t0]
    dispatch(9)
end

_llint_op_put_by_id_transition_direct:
    putByIdTransition(macro (oldStructure, scratch, slow) end)


_llint_op_put_by_id_transition_normal:
    putByIdTransition(
        macro (oldStructure, scratch, slow)
            const protoCell = oldStructure   # Reusing the oldStructure register for the proto
        
            loadp 28[PC], scratch
            assert(macro (ok) btpnz scratch, ok end)
            loadp StructureChain::m_vector[scratch], scratch
            assert(macro (ok) btpnz scratch, ok end)
            bieq Structure::m_prototype + TagOffset[oldStructure], NullTag, .done
        .loop:
            loadi Structure::m_prototype + PayloadOffset[oldStructure], protoCell
            loadp JSCell::m_structure[protoCell], oldStructure
            bpneq oldStructure, [scratch], slow
            addp 4, scratch
            bineq Structure::m_prototype + TagOffset[oldStructure], NullTag, .loop
        .done:
        end)


_llint_op_get_by_val:
    traceExecution()
    loadp CodeBlock[cfr], t1
    loadi 8[PC], t2
    loadi 12[PC], t3
    loadp CodeBlock::m_globalData[t1], t1
    loadConstantOrVariablePayload(t2, CellTag, t0, .opGetByValSlow)
    loadp JSGlobalData::jsArrayClassInfo[t1], t2
    loadConstantOrVariablePayload(t3, Int32Tag, t1, .opGetByValSlow)
    bpneq [t0], t2, .opGetByValSlow
    loadp JSArray::m_storage[t0], t3
    biaeq t1, JSArray::m_vectorLength[t0], .opGetByValSlow
    loadi 4[PC], t0
    loadi ArrayStorage::m_vector + TagOffset[t3, t1, 8], t2
    loadi ArrayStorage::m_vector + PayloadOffset[t3, t1, 8], t1
    bieq t2, EmptyValueTag, .opGetByValSlow
    storei t2, TagOffset[cfr, t0, 8]
    storei t1, PayloadOffset[cfr, t0, 8]
    loadi 16[PC], t0
    valueProfile(t2, t1, t0)
    dispatch(5)

.opGetByValSlow:
    callSlowPath(_llint_slow_path_get_by_val)
    dispatch(5)


_llint_op_get_argument_by_val:
    traceExecution()
    loadi 8[PC], t0
    loadi 12[PC], t1
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opGetArgumentByValSlow
    loadConstantOrVariablePayload(t1, Int32Tag, t2, .opGetArgumentByValSlow)
    addi 1, t2
    loadi ArgumentCount + PayloadOffset[cfr], t1
    biaeq t2, t1, .opGetArgumentByValSlow
    negi t2
    loadi 4[PC], t3
    loadi ThisArgumentOffset + TagOffset[cfr, t2, 8], t0
    loadi ThisArgumentOffset + PayloadOffset[cfr, t2, 8], t1
    storei t0, TagOffset[cfr, t3, 8]
    storei t1, PayloadOffset[cfr, t3, 8]
    dispatch(5)

.opGetArgumentByValSlow:
    callSlowPath(_llint_slow_path_get_argument_by_val)
    dispatch(5)


_llint_op_get_by_pname:
    traceExecution()
    loadi 12[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t1, .opGetByPnameSlow)
    loadi 16[PC], t0
    bpneq t1, PayloadOffset[cfr, t0, 8], .opGetByPnameSlow
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t2, .opGetByPnameSlow)
    loadi 20[PC], t0
    loadi PayloadOffset[cfr, t0, 8], t3
    loadp JSCell::m_structure[t2], t0
    bpneq t0, JSPropertyNameIterator::m_cachedStructure[t3], .opGetByPnameSlow
    loadi 24[PC], t0
    loadi [cfr, t0, 8], t0
    subi 1, t0
    biaeq t0, JSPropertyNameIterator::m_numCacheableSlots[t3], .opGetByPnameSlow
    loadp JSObject::m_propertyStorage[t2], t2
    loadi TagOffset[t2, t0, 8], t1
    loadi PayloadOffset[t2, t0, 8], t3
    loadi 4[PC], t0
    storei t1, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(7)

.opGetByPnameSlow:
    callSlowPath(_llint_slow_path_get_by_pname)
    dispatch(7)


_llint_op_put_by_val:
    traceExecution()
    loadi 4[PC], t0
    loadConstantOrVariablePayload(t0, CellTag, t1, .opPutByValSlow)
    loadi 8[PC], t0
    loadConstantOrVariablePayload(t0, Int32Tag, t2, .opPutByValSlow)
    loadp CodeBlock[cfr], t0
    loadp CodeBlock::m_globalData[t0], t0
    loadp JSGlobalData::jsArrayClassInfo[t0], t0
    bpneq [t1], t0, .opPutByValSlow
    biaeq t2, JSArray::m_vectorLength[t1], .opPutByValSlow
    loadp JSArray::m_storage[t1], t0
    bieq ArrayStorage::m_vector + TagOffset[t0, t2, 8], EmptyValueTag, .opPutByValEmpty
.opPutByValStoreResult:
    loadi 12[PC], t3
    loadConstantOrVariable2Reg(t3, t1, t3)
    writeBarrier(t1, t3)
    storei t1, ArrayStorage::m_vector + TagOffset[t0, t2, 8]
    storei t3, ArrayStorage::m_vector + PayloadOffset[t0, t2, 8]
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
    dispatchBranch(4[PC])


macro jumpTrueOrFalse(conditionOp, slow)
    loadi 4[PC], t1
    loadConstantOrVariablePayload(t1, BooleanTag, t0, .slow)
    conditionOp(t0, .target)
    dispatch(3)

.target:
    dispatchBranch(8[PC])

.slow:
    callSlowPath(slow)
    dispatch(0)
end


macro equalNull(cellHandler, immediateHandler)
    loadi 4[PC], t0
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
    bineq t1, CellTag, .immediate
    loadp JSCell::m_structure[t0], t2
    cellHandler(Structure::m_typeInfo + TypeInfo::m_flags[t2], .target)
    dispatch(3)

.target:
    dispatchBranch(8[PC])

.immediate:
    ori 1, t1
    immediateHandler(t1, .target)
    dispatch(3)
end

_llint_op_jeq_null:
    traceExecution()
    equalNull(
        macro (value, target) btbnz value, MasqueradesAsUndefined, target end,
        macro (value, target) bieq value, NullTag, target end)
    

_llint_op_jneq_null:
    traceExecution()
    equalNull(
        macro (value, target) btbz value, MasqueradesAsUndefined, target end,
        macro (value, target) bineq value, NullTag, target end)


_llint_op_jneq_ptr:
    traceExecution()
    loadi 4[PC], t0
    loadi 8[PC], t1
    bineq TagOffset[cfr, t0, 8], CellTag, .opJneqPtrBranch
    bpeq PayloadOffset[cfr, t0, 8], t1, .opJneqPtrFallThrough
.opJneqPtrBranch:
    dispatchBranch(12[PC])
.opJneqPtrFallThrough:
    dispatch(4)


macro compare(integerCompare, doubleCompare, slowPath)
    loadi 4[PC], t2
    loadi 8[PC], t3
    loadConstantOrVariable(t2, t0, t1)
    loadConstantOrVariable2Reg(t3, t2, t3)
    bineq t0, Int32Tag, .op1NotInt
    bineq t2, Int32Tag, .op2NotInt
    integerCompare(t1, t3, .jumpTarget)
    dispatch(4)

.op1NotInt:
    bia t0, LowestTag, .slow
    bib t2, LowestTag, .op1NotIntOp2Double
    bineq t2, Int32Tag, .slow
    ci2d t3, ft1
    jmp .op1NotIntReady
.op1NotIntOp2Double:
    fii2d t3, t2, ft1
.op1NotIntReady:
    fii2d t1, t0, ft0
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.op2NotInt:
    ci2d t1, ft0
    bia t2, LowestTag, .slow
    fii2d t3, t2, ft1
    doubleCompare(ft0, ft1, .jumpTarget)
    dispatch(4)

.jumpTarget:
    dispatchBranch(12[PC])

.slow:
    callSlowPath(slowPath)
    dispatch(0)
end


_llint_op_switch_imm:
    traceExecution()
    loadi 12[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3   # FIXME: would be nice to peephole this!
    loadp CodeBlock::RareData::m_immediateSwitchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bineq t1, Int32Tag, .opSwitchImmNotInt
    subi SimpleJumpTable::min[t2], t0
    biaeq t0, SimpleJumpTable::branchOffsets + VectorSizeOffset[t2], .opSwitchImmFallThrough
    loadp SimpleJumpTable::branchOffsets + VectorBufferOffset[t2], t3
    loadi [t3, t0, 4], t1
    btiz t1, .opSwitchImmFallThrough
    dispatchBranchWithOffset(t1)

.opSwitchImmNotInt:
    bib t1, LowestTag, .opSwitchImmSlow  # Go to slow path if it's a double.
.opSwitchImmFallThrough:
    dispatchBranch(8[PC])

.opSwitchImmSlow:
    callSlowPath(_llint_slow_path_switch_imm)
    dispatch(0)


_llint_op_switch_char:
    traceExecution()
    loadi 12[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
    loadp CodeBlock[cfr], t2
    loadp CodeBlock::m_rareData[t2], t2
    muli sizeof SimpleJumpTable, t3
    loadp CodeBlock::RareData::m_characterSwitchJumpTables + VectorBufferOffset[t2], t2
    addp t3, t2
    bineq t1, CellTag, .opSwitchCharFallThrough
    loadp JSCell::m_structure[t0], t1
    bbneq Structure::m_typeInfo + TypeInfo::m_type[t1], StringType, .opSwitchCharFallThrough
    loadp JSString::m_value[t0], t0
    bineq StringImpl::m_length[t0], 1, .opSwitchCharFallThrough
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
    loadi [t2, t0, 4], t1
    btiz t1, .opSwitchCharFallThrough
    dispatchBranchWithOffset(t1)

.opSwitchCharFallThrough:
    dispatchBranch(8[PC])


_llint_op_new_func:
    traceExecution()
    btiz 12[PC], .opNewFuncUnchecked
    loadi 4[PC], t1
    bineq TagOffset[cfr, t1, 8], EmptyValueTag, .opNewFuncDone
.opNewFuncUnchecked:
    callSlowPath(_llint_slow_path_new_func)
.opNewFuncDone:
    dispatch(4)


macro doCall(slowPath)
    loadi 4[PC], t0
    loadi 16[PC], t1
    loadp LLIntCallLinkInfo::callee[t1], t2
    loadConstantOrVariablePayload(t0, CellTag, t3, .opCallSlow)
    bineq t3, t2, .opCallSlow
    loadi 12[PC], t3
    addp 24, PC
    lshifti 3, t3
    addp cfr, t3  # t3 contains the new value of cfr
    loadp JSFunction::m_scopeChain[t2], t0
    storei t2, Callee + PayloadOffset[t3]
    storei t0, ScopeChain + PayloadOffset[t3]
    loadi 8 - 24[PC], t2
    storei PC, ArgumentCount + TagOffset[cfr]
    storep cfr, CallerFrame[t3]
    storei t2, ArgumentCount + PayloadOffset[t3]
    storei CellTag, Callee + TagOffset[t3]
    storei CellTag, ScopeChain + TagOffset[t3]
    move t3, cfr
    call LLIntCallLinkInfo::machineCodeTarget[t1]
    dispatchAfterCall()

.opCallSlow:
    slowPathForCall(6, slowPath)
end


_llint_op_tear_off_activation:
    traceExecution()
    loadi 4[PC], t0
    loadi 8[PC], t1
    bineq TagOffset[cfr, t0, 8], EmptyValueTag, .opTearOffActivationCreated
    bieq TagOffset[cfr, t1, 8], EmptyValueTag, .opTearOffActivationNotCreated
.opTearOffActivationCreated:
    callSlowPath(_llint_slow_path_tear_off_activation)
.opTearOffActivationNotCreated:
    dispatch(3)


_llint_op_tear_off_arguments:
    traceExecution()
    loadi 4[PC], t0
    subi 1, t0   # Get the unmodifiedArgumentsRegister
    bieq TagOffset[cfr, t0, 8], EmptyValueTag, .opTearOffArgumentsNotCreated
    callSlowPath(_llint_slow_path_tear_off_arguments)
.opTearOffArgumentsNotCreated:
    dispatch(2)


_llint_op_ret:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    doReturn()


_llint_op_call_put_result:
    loadi 4[PC], t2
    loadi 8[PC], t3
    storei t1, TagOffset[cfr, t2, 8]
    storei t0, PayloadOffset[cfr, t2, 8]
    valueProfile(t1, t0, t3)
    traceExecution() # Needs to be here because it would clobber t1, t0
    dispatch(3)


_llint_op_ret_object_or_this:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    bineq t1, CellTag, .opRetObjectOrThisNotObject
    loadp JSCell::m_structure[t0], t2
    bbb Structure::m_typeInfo + TypeInfo::m_type[t2], ObjectType, .opRetObjectOrThisNotObject
    doReturn()

.opRetObjectOrThisNotObject:
    loadi 8[PC], t2
    loadConstantOrVariable(t2, t1, t0)
    doReturn()


_llint_op_to_primitive:
    traceExecution()
    loadi 8[PC], t2
    loadi 4[PC], t3
    loadConstantOrVariable(t2, t1, t0)
    bineq t1, CellTag, .opToPrimitiveIsImm
    loadp JSCell::m_structure[t0], t2
    bbneq Structure::m_typeInfo + TypeInfo::m_type[t2], StringType, .opToPrimitiveSlowCase
.opToPrimitiveIsImm:
    storei t1, TagOffset[cfr, t3, 8]
    storei t0, PayloadOffset[cfr, t3, 8]
    dispatch(3)

.opToPrimitiveSlowCase:
    callSlowPath(_llint_slow_path_to_primitive)
    dispatch(3)


_llint_op_next_pname:
    traceExecution()
    loadi 12[PC], t1
    loadi 16[PC], t2
    loadi PayloadOffset[cfr, t1, 8], t0
    bieq t0, PayloadOffset[cfr, t2, 8], .opNextPnameEnd
    loadi 20[PC], t2
    loadi PayloadOffset[cfr, t2, 8], t2
    loadp JSPropertyNameIterator::m_jsStrings[t2], t3
    loadi [t3, t0, 8], t3
    addi 1, t0
    storei t0, PayloadOffset[cfr, t1, 8]
    loadi 4[PC], t1
    storei CellTag, TagOffset[cfr, t1, 8]
    storei t3, PayloadOffset[cfr, t1, 8]
    loadi 8[PC], t3
    loadi PayloadOffset[cfr, t3, 8], t3
    loadp JSCell::m_structure[t3], t1
    bpneq t1, JSPropertyNameIterator::m_cachedStructure[t2], .opNextPnameSlow
    loadp JSPropertyNameIterator::m_cachedPrototypeChain[t2], t0
    loadp StructureChain::m_vector[t0], t0
    btpz [t0], .opNextPnameTarget
.opNextPnameCheckPrototypeLoop:
    bieq Structure::m_prototype + TagOffset[t1], NullTag, .opNextPnameSlow
    loadp Structure::m_prototype + PayloadOffset[t1], t2
    loadp JSCell::m_structure[t2], t1
    bpneq t1, [t0], .opNextPnameSlow
    addp 4, t0
    btpnz [t0], .opNextPnameCheckPrototypeLoop
.opNextPnameTarget:
    dispatchBranch(24[PC])

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
    loadp JITStackFrame::globalData[sp], t3
    loadi JSGlobalData::targetInterpreterPCForThrow[t3], PC
    loadi JSGlobalData::exception + PayloadOffset[t3], t0
    loadi JSGlobalData::exception + TagOffset[t3], t1
    storei 0, JSGlobalData::exception + PayloadOffset[t3]
    storei EmptyValueTag, JSGlobalData::exception + TagOffset[t3]       
    loadi 4[PC], t2
    storei t0, PayloadOffset[cfr, t2, 8]
    storei t1, TagOffset[cfr, t2, 8]
    traceExecution()  # This needs to be here because we don't want to clobber t0, t1, t2, t3 above.
    dispatch(2)


_llint_op_end:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t0
    assertNotConstant(t0)
    loadi TagOffset[cfr, t0, 8], t1
    loadi PayloadOffset[cfr, t0, 8], t0
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
    loadp CallerFrame[cfr], t0
    loadi ScopeChain + PayloadOffset[t0], t1
    storei CellTag, ScopeChain + TagOffset[cfr]
    storei t1, ScopeChain + PayloadOffset[cfr]
    if X86
        loadp JITStackFrame::globalData + 4[sp], t0 # Additional offset for return address
        storep cfr, JSGlobalData::topCallFrame[t0]
        peek 0, t1
        storep t1, ReturnPC[cfr]
        move cfr, t2  # t2 = ecx
        subp 16 - 4, sp
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        move t0, cfr
        call executableOffsetToFunction[t1]
        addp 16 - 4, sp
        loadp JITStackFrame::globalData + 4[sp], t3
    elsif ARMv7
        loadp JITStackFrame::globalData[sp], t1
        storep cfr, JSGlobalData::topCallFrame[t1]
        move t0, t2
        preserveReturnAddressAfterCall(t3)
        storep t3, ReturnPC[cfr]
        move cfr, t0
        loadi Callee + PayloadOffset[cfr], t1
        loadp JSFunction::m_executable[t1], t1
        move t2, cfr
        call executableOffsetToFunction[t1]
        restoreReturnAddressBeforeReturn(t3)
        loadp JITStackFrame::globalData[sp], t3
    else  
        error
    end
    bineq JSGlobalData::exception + TagOffset[t3], EmptyValueTag, .exception
    ret
.exception:
    preserveReturnAddressAfterCall(t1) # This is really only needed on X86
    loadi ArgumentCount + TagOffset[cfr], PC
    callSlowPath(_llint_throw_from_native_call)
    jmp _llint_throw_from_slow_path_trampoline
end

