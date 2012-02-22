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

# These declarations must match interpreter/RegisterFile.h.
const CallFrameHeaderSize = 48
const ArgumentCount = -48
const CallerFrame = -40
const Callee = -32
const ScopeChain = -24
const ReturnPC = -16
const CodeBlock = -8

const ThisArgumentOffset = -CallFrameHeaderSize - 8

# Declare some aliases for the registers we will use.
const PC = t4

# Offsets needed for reasoning about value representation.
if BIG_ENDIAN
    const TagOffset = 0
    const PayloadOffset = 4
else
    const TagOffset = 4
    const PayloadOffset = 0
end

# Value representation constants.
const Int32Tag = -1
const BooleanTag = -2
const NullTag = -3
const UndefinedTag = -4
const CellTag = -5
const EmptyValueTag = -6
const DeletedValueTag = -7
const LowestTag = DeletedValueTag

# Type constants.
const StringType = 5
const ObjectType = 13

# Type flags constants.
const MasqueradesAsUndefined = 1
const ImplementsHasInstance = 2
const ImplementsDefaultHasInstance = 8

# Heap allocation constants.
const JSFinalObjectSizeClassIndex = 3

# Bytecode operand constants.
const FirstConstantRegisterIndex = 0x40000000

# Code type constants.
const GlobalCode = 0
const EvalCode = 1
const FunctionCode = 2

# The interpreter steals the tag word of the argument count.
const LLIntReturnPC = ArgumentCount + TagOffset

# This must match wtf/Vector.h.
const VectorSizeOffset = 0
const VectorBufferOffset = 4

# String flags.
const HashFlags8BitBuffer = 64

# Utilities
macro crash()
    storei 0, 0xbbadbeef[]
    move 0, t0
    call t0
end

macro assert(assertion)
    if ASSERT_ENABLED
        assertion(.ok)
        crash()
    .ok:
    end
end

macro preserveReturnAddressAfterCall(destinationRegister)
    if ARMv7
        move lr, destinationRegister
    elsif X86
        pop destinationRegister
    else
        error
    end
end

macro restoreReturnAddressBeforeReturn(sourceRegister)
    if ARMv7
        move sourceRegister, lr
    elsif X86
        push sourceRegister
    else
        error
    end
end

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

macro callSlowPath(slow_path)
    cCall2(slow_path, cfr, PC)
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

macro traceExecution()
    if EXECUTION_TRACING
        callSlowPath(_llint_trace)
    end
end

# Call a slow_path for call opcodes.
macro callCallSlowPath(advance, slow_path, action)
    addp advance * 4, PC, t0
    storep t0, ArgumentCount + TagOffset[cfr]
    cCall2(slow_path, cfr, PC)
    move t1, cfr
    action(t0)
end

macro slowPathForCall(advance, slow_path)
    callCallSlowPath(
        advance,
        slow_path,
        macro (callee)
            call callee
            dispatchAfterCall()
        end)
end

macro checkSwitchToJIT(increment, action)
    if JIT_ENABLED
        loadp CodeBlock[cfr], t0
        baddis increment, CodeBlock::m_llintExecuteCounter[t0], .continue
        action()
    .continue:
    end
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

macro checkSwitchToJITForEpilogue()
    checkSwitchToJIT(
        10,
        macro ()
            callSlowPath(_llint_replace)
        end)
end

macro assertNotConstant(index)
    assert(macro (ok) bilt index, FirstConstantRegisterIndex, ok end)
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
    if JIT_ENABLED
        storei tag, ValueProfile::m_buckets + TagOffset[profile]
        storei payload, ValueProfile::m_buckets + PayloadOffset[profile]
    end
end


# Indicate the beginning of LLInt.
_llint_begin:
    crash()


# Entrypoints into the interpreter

macro functionForCallCodeBlockGetter(targetRegister)
    loadp Callee[cfr], targetRegister
    loadp JSFunction::m_executable[targetRegister], targetRegister
    loadp FunctionExecutable::m_codeBlockForCall[targetRegister], targetRegister
end

macro functionForConstructCodeBlockGetter(targetRegister)
    loadp Callee[cfr], targetRegister
    loadp JSFunction::m_executable[targetRegister], targetRegister
    loadp FunctionExecutable::m_codeBlockForConstruct[targetRegister], targetRegister
end

macro notFunctionCodeBlockGetter(targetRegister)
    loadp CodeBlock[cfr], targetRegister
end

macro functionCodeBlockSetter(sourceRegister)
    storep sourceRegister, CodeBlock[cfr]
end

macro notFunctionCodeBlockSetter(sourceRegister)
    # Nothing to do!
end

# Do the bare minimum required to execute code. Sets up the PC, leave the CodeBlock*
# in t1. May also trigger prologue entry OSR.
macro prologue(codeBlockGetter, codeBlockSetter, osrSlowPath, traceSlowPath)
    preserveReturnAddressAfterCall(t2)
    
    # Set up the call frame and check if we should OSR.
    storep t2, ReturnPC[cfr]
    if EXECUTION_TRACING
        callSlowPath(traceSlowPath)
    end
    codeBlockGetter(t1)
    if JIT_ENABLED
        baddis 5, CodeBlock::m_llintExecuteCounter[t1], .continue
        cCall2(osrSlowPath, cfr, PC)
        move t1, cfr
        btpz t0, .recover
        loadp ReturnPC[cfr], t2
        restoreReturnAddressBeforeReturn(t2)
        jmp t0
    .recover:
        codeBlockGetter(t1)
    .continue:
    end
    codeBlockSetter(t1)
    
    # Set up the PC.
    loadp CodeBlock::m_instructions[t1], t0
    loadp CodeBlock::Instructions::m_instructions + VectorBufferOffset[t0], PC
end

# Expects that CodeBlock is in t1, which is what prologue() leaves behind.
# Must call dispatch(0) after calling this.
macro functionInitialization(profileArgSkip)
    if JIT_ENABLED
        # Profile the arguments. Unfortunately, we have no choice but to do this. This
        # code is pretty horrendous because of the difference in ordering between
        # arguments and value profiles, the desire to have a simple loop-down-to-zero
        # loop, and the desire to use only three registers so as to preserve the PC and
        # the code block. It is likely that this code should be rewritten in a more
        # optimal way for architectures that have more than five registers available
        # for arbitrary use in the interpreter.
        loadi CodeBlock::m_numParameters[t1], t0
        addi -profileArgSkip, t0 # Use addi because that's what has the peephole
        assert(macro (ok) bigteq t0, 0, ok end)
        btiz t0, .argumentProfileDone
        loadp CodeBlock::m_argumentValueProfiles + VectorBufferOffset[t1], t3
        muli sizeof ValueProfile, t0, t2 # Aaaaahhhh! Need strength reduction!
        negi t0
        lshifti 3, t0
        addp t2, t3
    .argumentProfileLoop:
        loadi ThisArgumentOffset + TagOffset + 8 - profileArgSkip * 8[cfr, t0], t2
        subp sizeof ValueProfile, t3
        storei t2, profileArgSkip * sizeof ValueProfile + ValueProfile::m_buckets + TagOffset[t3]
        loadi ThisArgumentOffset + PayloadOffset + 8 - profileArgSkip * 8[cfr, t0], t2
        storei t2, profileArgSkip * sizeof ValueProfile + ValueProfile::m_buckets + PayloadOffset[t3]
        baddinz 8, t0, .argumentProfileLoop
    .argumentProfileDone:
    end
        
    # Check stack height.
    loadi CodeBlock::m_numCalleeRegisters[t1], t0
    loadp CodeBlock::m_globalData[t1], t2
    loadp JSGlobalData::interpreter[t2], t2   # FIXME: Can get to the RegisterFile from the JITStackFrame
    lshifti 3, t0
    addp t0, cfr, t0
    bpaeq Interpreter::m_registerFile + RegisterFile::m_end[t2], t0, .stackHeightOK

    # Stack height check failed - need to call a slow_path.
    callSlowPath(_llint_register_file_check)
.stackHeightOK:
end

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
    loadp CodeBlock::m_instructions[t1], t0
    loadp CodeBlock::Instructions::m_instructions + VectorBufferOffset[t0], PC
    jmp doneLabel
end

_llint_program_prologue:
    prologue(notFunctionCodeBlockGetter, notFunctionCodeBlockSetter, _llint_entry_osr, _llint_trace_prologue)
    dispatch(0)


_llint_eval_prologue:
    prologue(notFunctionCodeBlockGetter, notFunctionCodeBlockSetter, _llint_entry_osr, _llint_trace_prologue)
    dispatch(0)


_llint_function_for_call_prologue:
    prologue(functionForCallCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_call, _llint_trace_prologue_function_for_call)
.functionForCallBegin:
    functionInitialization(0)
    dispatch(0)
    

_llint_function_for_construct_prologue:
    prologue(functionForConstructCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_construct, _llint_trace_prologue_function_for_construct)
.functionForConstructBegin:
    functionInitialization(1)
    dispatch(0)
    

_llint_function_for_call_arity_check:
    prologue(functionForCallCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_call_arityCheck, _llint_trace_arityCheck_for_call)
    functionArityCheck(.functionForCallBegin, _llint_slow_path_call_arityCheck)


_llint_function_for_construct_arity_check:
    prologue(functionForConstructCodeBlockGetter, functionCodeBlockSetter, _llint_entry_osr_function_for_construct_arityCheck, _llint_trace_arityCheck_for_construct)
    functionArityCheck(.functionForConstructBegin, _llint_slow_path_construct_arityCheck)

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


macro allocateBasicJSObject(sizeClassIndex, classInfoOffset, structure, result, scratch1, scratch2, slowCase)
    if ALWAYS_ALLOCATE_SLOW
        jmp slowCase
    else
        const offsetOfMySizeClass =
            JSGlobalData::heap +
            Heap::m_objectSpace +
            MarkedSpace::m_normalSpace +
            MarkedSpace::Subspace::preciseAllocators +
            sizeClassIndex * sizeof MarkedAllocator
        
        # FIXME: we can get the global data in one load from the stack.
        loadp CodeBlock[cfr], scratch1
        loadp CodeBlock::m_globalData[scratch1], scratch1
        
        # Get the object from the free list.    
        loadp offsetOfMySizeClass + MarkedAllocator::m_firstFreeCell[scratch1], result
        btpz result, slowCase
        
        # Remove the object from the free list.
        loadp [result], scratch2
        storep scratch2, offsetOfMySizeClass + MarkedAllocator::m_firstFreeCell[scratch1]
    
        # Initialize the object.
        loadp classInfoOffset[scratch1], scratch2
        storep scratch2, [result]
        storep structure, JSCell::m_structure[result]
        storep 0, JSObject::m_inheritorID[result]
        addp sizeof JSObject, result, scratch1
        storep scratch1, JSObject::m_propertyStorage[result]
    end
end

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


_llint_op_new_array:
    traceExecution()
    callSlowPath(_llint_slow_path_new_array)
    dispatch(4)


_llint_op_new_array_buffer:
    traceExecution()
    callSlowPath(_llint_slow_path_new_array_buffer)
    dispatch(4)


_llint_op_new_regexp:
    traceExecution()
    callSlowPath(_llint_slow_path_new_regexp)
    dispatch(3)


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


macro strictEq(equalityOperation, slow_path)
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
    callSlowPath(slow_path)
    dispatch(4)
end

_llint_op_stricteq:
    traceExecution()
    strictEq(macro (left, right, result) cieq left, right, result end, _llint_slow_path_stricteq)


_llint_op_nstricteq:
    traceExecution()
    strictEq(macro (left, right, result) cineq left, right, result end, _llint_slow_path_nstricteq)


_llint_op_less:
    traceExecution()
    callSlowPath(_llint_slow_path_less)
    dispatch(4)


_llint_op_lesseq:
    traceExecution()
    callSlowPath(_llint_slow_path_lesseq)
    dispatch(4)


_llint_op_greater:
    traceExecution()
    callSlowPath(_llint_slow_path_greater)
    dispatch(4)


_llint_op_greatereq:
    traceExecution()
    callSlowPath(_llint_slow_path_greatereq)
    dispatch(4)


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


macro binaryOpCustomStore(integerOperationAndStore, doubleOperation, slow_path)
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
    callSlowPath(slow_path)
    dispatch(5)
end

macro binaryOp(integerOperation, doubleOperation, slow_path)
    binaryOpCustomStore(
        macro (int32Tag, left, right, slow, index)
            integerOperation(left, right, slow)
            storei int32Tag, TagOffset[cfr, index, 8]
            storei right, PayloadOffset[cfr, index, 8]
        end,
        doubleOperation, slow_path)
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


_llint_op_mod:
    traceExecution()
    callSlowPath(_llint_slow_path_mod)
    dispatch(4)


macro bitOp(operation, slow_path, advance)
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
    callSlowPath(slow_path)
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


_llint_op_bitnot:
    traceExecution()
    loadi 8[PC], t1
    loadi 4[PC], t0
    loadConstantOrVariable(t1, t2, t3)
    bineq t2, Int32Tag, .opBitnotSlow
    noti t3
    storei t2, TagOffset[cfr, t0, 8]
    storei t3, PayloadOffset[cfr, t0, 8]
    dispatch(3)

.opBitnotSlow:
    callSlowPath(_llint_slow_path_bitnot)
    dispatch(3)


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


_llint_op_typeof:
    traceExecution()
    callSlowPath(_llint_slow_path_typeof)
    dispatch(3)


_llint_op_is_undefined:
    traceExecution()
    callSlowPath(_llint_slow_path_is_undefined)
    dispatch(3)


_llint_op_is_boolean:
    traceExecution()
    callSlowPath(_llint_slow_path_is_boolean)
    dispatch(3)


_llint_op_is_number:
    traceExecution()
    callSlowPath(_llint_slow_path_is_number)
    dispatch(3)


_llint_op_is_string:
    traceExecution()
    callSlowPath(_llint_slow_path_is_string)
    dispatch(3)


_llint_op_is_object:
    traceExecution()
    callSlowPath(_llint_slow_path_is_object)
    dispatch(3)


_llint_op_is_function:
    traceExecution()
    callSlowPath(_llint_slow_path_is_function)
    dispatch(3)


_llint_op_in:
    traceExecution()
    callSlowPath(_llint_slow_path_in)
    dispatch(4)


_llint_op_resolve:
    traceExecution()
    callSlowPath(_llint_slow_path_resolve)
    dispatch(4)


_llint_op_resolve_skip:
    traceExecution()
    callSlowPath(_llint_slow_path_resolve_skip)
    dispatch(5)


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


_llint_op_resolve_base:
    traceExecution()
    callSlowPath(_llint_slow_path_resolve_base)
    dispatch(5)


_llint_op_ensure_property_exists:
    traceExecution()
    callSlowPath(_llint_slow_path_ensure_property_exists)
    dispatch(3)


_llint_op_resolve_with_base:
    traceExecution()
    callSlowPath(_llint_slow_path_resolve_with_base)
    dispatch(5)


_llint_op_resolve_with_this:
    traceExecution()
    callSlowPath(_llint_slow_path_resolve_with_this)
    dispatch(5)


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


_llint_op_del_by_id:
    traceExecution()
    callSlowPath(_llint_slow_path_del_by_id)
    dispatch(4)


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


_llint_op_del_by_val:
    traceExecution()
    callSlowPath(_llint_slow_path_del_by_val)
    dispatch(4)


_llint_op_put_by_index:
    traceExecution()
    callSlowPath(_llint_slow_path_put_by_index)
    dispatch(4)


_llint_op_put_getter_setter:
    traceExecution()
    callSlowPath(_llint_slow_path_put_getter_setter)
    dispatch(5)


_llint_op_loop:
    nop
_llint_op_jmp:
    traceExecution()
    dispatchBranch(4[PC])


_llint_op_jmp_scopes:
    traceExecution()
    callSlowPath(_llint_slow_path_jmp_scopes)
    dispatch(0)


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

_llint_op_loop_if_true:
    nop
_llint_op_jtrue:
    traceExecution()
    jumpTrueOrFalse(
        macro (value, target) btinz value, target end,
        _llint_slow_path_jtrue)


_llint_op_loop_if_false:
    nop
_llint_op_jfalse:
    traceExecution()
    jumpTrueOrFalse(
        macro (value, target) btiz value, target end,
        _llint_slow_path_jfalse)


macro equalNull(cellHandler, immediateHandler)
    loadi 4[PC], t0
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


macro compare(integerCompare, doubleCompare, slow_path)
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
    callSlowPath(slow_path)
    dispatch(0)
end

_llint_op_loop_if_less:
    nop
_llint_op_jless:
    traceExecution()
    compare(
        macro (left, right, target) bilt left, right, target end,
        macro (left, right, target) bdlt left, right, target end,
        _llint_slow_path_jless)


_llint_op_jnless:
    traceExecution()
    compare(
        macro (left, right, target) bigteq left, right, target end,
        macro (left, right, target) bdgtequn left, right, target end,
        _llint_slow_path_jnless)


_llint_op_loop_if_greater:
    nop
_llint_op_jgreater:
    traceExecution()
    compare(
        macro (left, right, target) bigt left, right, target end,
        macro (left, right, target) bdgt left, right, target end,
        _llint_slow_path_jgreater)


_llint_op_jngreater:
    traceExecution()
    compare(
        macro (left, right, target) bilteq left, right, target end,
        macro (left, right, target) bdltequn left, right, target end,
        _llint_slow_path_jngreater)


_llint_op_loop_if_lesseq:
    nop
_llint_op_jlesseq:
    traceExecution()
    compare(
        macro (left, right, target) bilteq left, right, target end,
        macro (left, right, target) bdlteq left, right, target end,
        _llint_slow_path_jlesseq)


_llint_op_jnlesseq:
    traceExecution()
    compare(
        macro (left, right, target) bigt left, right, target end,
        macro (left, right, target) bdgtun left, right, target end,
        _llint_slow_path_jnlesseq)


_llint_op_loop_if_greatereq:
    nop
_llint_op_jgreatereq:
    traceExecution()
    compare(
        macro (left, right, target) bigteq left, right, target end,
        macro (left, right, target) bdgteq left, right, target end,
        _llint_slow_path_jgreatereq)


_llint_op_jngreatereq:
    traceExecution()
    compare(
        macro (left, right, target) bilt left, right, target end,
        macro (left, right, target) bdltun left, right, target end,
        _llint_slow_path_jngreatereq)


_llint_op_loop_hint:
    traceExecution()
    checkSwitchToJITForLoop()
    dispatch(1)


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
    btiz t1, .opSwitchImmFallThrough
    dispatchBranchWithOffset(t1)

.opSwitchCharFallThrough:
    dispatchBranch(8[PC])


_llint_op_switch_string:
    traceExecution()
    callSlowPath(_llint_slow_path_switch_string)
    dispatch(0)


_llint_op_new_func:
    traceExecution()
    btiz 12[PC], .opNewFuncUnchecked
    loadi 4[PC], t1
    bineq TagOffset[cfr, t1, 8], EmptyValueTag, .opNewFuncDone
.opNewFuncUnchecked:
    callSlowPath(_llint_slow_path_new_func)
.opNewFuncDone:
    dispatch(4)


_llint_op_new_func_exp:
    traceExecution()
    callSlowPath(_llint_slow_path_new_func_exp)
    dispatch(3)


macro doCall(slow_path)
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
    slowPathForCall(6, slow_path)
end

_llint_op_call:
    traceExecution()
    doCall(_llint_slow_path_call)


_llint_op_construct:
    traceExecution()
    doCall(_llint_slow_path_construct)


_llint_op_call_varargs:
    traceExecution()
    slowPathForCall(6, _llint_slow_path_call_varargs)


_llint_op_call_eval:
    traceExecution()
    
    # Eval is executed in one of two modes:
    #
    # 1) We find that we're really invoking eval() in which case the
    #    execution is perfomed entirely inside the slow_path, and it
    #    returns the PC of a function that just returns the return value
    #    that the eval returned.
    #
    # 2) We find that we're invoking something called eval() that is not
    #    the real eval. Then the slow_path returns the PC of the thing to
    #    call, and we call it.
    #
    # This allows us to handle two cases, which would require a total of
    # up to four pieces of state that cannot be easily packed into two
    # registers (C functions can return up to two registers, easily):
    #
    # - The call frame register. This may or may not have been modified
    #   by the slow_path, but the convention is that it returns it. It's not
    #   totally clear if that's necessary, since the cfr is callee save.
    #   But that's our style in this here interpreter so we stick with it.
    #
    # - A bit to say if the slow_path successfully executed the eval and has
    #   the return value, or did not execute the eval but has a PC for us
    #   to call.
    #
    # - Either:
    #   - The JS return value (two registers), or
    #
    #   - The PC to call.
    #
    # It turns out to be easier to just always have this return the cfr
    # and a PC to call, and that PC may be a dummy thunk that just
    # returns the JS value that the eval returned.
    
    slowPathForCall(4, _llint_slow_path_call_eval)


_llint_generic_return_point:
    dispatchAfterCall()


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


macro doReturn()
    loadp ReturnPC[cfr], t2
    loadp CallerFrame[cfr], cfr
    restoreReturnAddressBeforeReturn(t2)
    ret
end

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


_llint_op_method_check:
    traceExecution()
    # We ignore method checks and use normal get_by_id optimizations.
    dispatch(1)


_llint_op_strcat:
    traceExecution()
    callSlowPath(_llint_slow_path_strcat)
    dispatch(4)


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


_llint_op_get_pnames:
    traceExecution()
    callSlowPath(_llint_slow_path_get_pnames)
    dispatch(0) # The slow_path either advances the PC or jumps us to somewhere else.


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


_llint_op_push_scope:
    traceExecution()
    callSlowPath(_llint_slow_path_push_scope)
    dispatch(2)


_llint_op_pop_scope:
    traceExecution()
    callSlowPath(_llint_slow_path_pop_scope)
    dispatch(1)


_llint_op_push_new_scope:
    traceExecution()
    callSlowPath(_llint_slow_path_push_new_scope)
    dispatch(4)


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


_llint_op_throw:
    traceExecution()
    callSlowPath(_llint_slow_path_throw)
    dispatch(2)


_llint_op_throw_reference_error:
    traceExecution()
    callSlowPath(_llint_slow_path_throw_reference_error)
    dispatch(2)


_llint_op_jsr:
    traceExecution()
    loadi 4[PC], t0
    addi 3 * 4, PC, t1
    storei t1, [cfr, t0, 8]
    dispatchBranch(8[PC])


_llint_op_sret:
    traceExecution()
    loadi 4[PC], t0
    loadp [cfr, t0, 8], PC
    dispatch(0)


_llint_op_debug:
    traceExecution()
    callSlowPath(_llint_slow_path_debug)
    dispatch(4)


_llint_op_profile_will_call:
    traceExecution()
    loadp JITStackFrame::enabledProfilerReference[sp], t0
    btpz [t0], .opProfileWillCallDone
    callSlowPath(_llint_slow_path_profile_will_call)
.opProfileWillCallDone:
    dispatch(2)


_llint_op_profile_did_call:
    traceExecution()
    loadp JITStackFrame::enabledProfilerReference[sp], t0
    btpz [t0], .opProfileWillCallDone
    callSlowPath(_llint_slow_path_profile_did_call)
.opProfileDidCallDone:
    dispatch(2)


_llint_op_end:
    traceExecution()
    checkSwitchToJITForEpilogue()
    loadi 4[PC], t0
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


# Lastly, make sure that we can link even though we don't support all opcodes.
# These opcodes should never arise when using LLInt or either JIT. We assert
# as much.

macro notSupported()
    if ASSERT_ENABLED
        crash()
    else
        # We should use whatever the smallest possible instruction is, just to
        # ensure that there is a gap between instruction labels. If multiple
        # smallest instructions exist, we should pick the one that is most
        # likely result in execution being halted. Currently that is the break
        # instruction on all architectures we're interested in. (Break is int3
        # on Intel, which is 1 byte, and bkpt on ARMv7, which is 2 bytes.)
        break
    end
end

_llint_op_get_array_length:
    notSupported()

_llint_op_get_by_id_chain:
    notSupported()

_llint_op_get_by_id_custom_chain:
    notSupported()

_llint_op_get_by_id_custom_proto:
    notSupported()

_llint_op_get_by_id_custom_self:
    notSupported()

_llint_op_get_by_id_generic:
    notSupported()

_llint_op_get_by_id_getter_chain:
    notSupported()

_llint_op_get_by_id_getter_proto:
    notSupported()

_llint_op_get_by_id_getter_self:
    notSupported()

_llint_op_get_by_id_proto:
    notSupported()

_llint_op_get_by_id_self:
    notSupported()

_llint_op_get_string_length:
    notSupported()

_llint_op_put_by_id_generic:
    notSupported()

_llint_op_put_by_id_replace:
    notSupported()

_llint_op_put_by_id_transition:
    notSupported()


# Indicate the end of LLInt.
_llint_end:
    crash()

